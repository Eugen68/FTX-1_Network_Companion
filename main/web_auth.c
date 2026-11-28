/*
 * SPDX-License-Identifier: MIT
 */

/*
 * Access control for the HTTPS server: Basic authentication (web and OTA credentials), temporary lock-out
 * after repeated failures, the CSRF check for state changing requests, and the security / CSP headers.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "lwip/sockets.h"
#include "mbedtls/base64.h"
#include "mbedtls/constant_time.h"
#include "app_config.h"
#include "web_auth.h"

#define AUTH_FIELD_BYTES    128                       // decoded "user:password", zero padded for a fixed-size compare
#define AUTH_HEADER_BYTES   (AUTH_FIELD_BYTES * 4 / 3 + 16)
#define HOST_HEADER_BYTES   256
#define ORIGIN_HEADER_BYTES (HOST_HEADER_BYTES + 16)

#define LOCK_TABLE_SIZE     8
#define LOCK_AFTER_FAILS    5
#define LOCK_DURATION_US    (60LL * 1000000LL)
#define FAIL_MEMORY_US      (10LL * 60LL * 1000000LL) // forget a client that stayed quiet this long

// The longest decoded "user:password" must fit the fixed compare buffer
_Static_assert((APP_CONFIG_WEB_USER_MAX_LEN + 1 + APP_CONFIG_WEB_PASS_MAX_LEN + 1) <= AUTH_FIELD_BYTES, "web credentials field is too small");
_Static_assert((APP_CONFIG_OTA_USER_MAX_LEN + 1 + APP_CONFIG_OTA_PASS_MAX_LEN + 1) <= AUTH_FIELD_BYTES, "OTA credentials field is too small");

// The embedded pages use inline scripts and styles and Google Fonts. The FT8 and CW decoders run in
// Web Workers created from Blob URLs (worker-src blob:). The CW decoder downloads its ONNX runtime
// and models from cdn.jsdelivr.net (too large for the firmware), checks their SHA-256, then imports
// the runtime from a blob: URL and compiles its WebAssembly ('wasm-unsafe-eval'). The AI noise reduction
// (HamNoise) does the same with its WebAssembly, in an AudioWorklet added from a blob: URL. Everything else
// (connections, frames, forms, <base>, plugins) is restricted to this device.
#define CONTENT_SECURITY_POLICY                                                         \
    "default-src 'none'; script-src 'self' 'unsafe-inline' 'wasm-unsafe-eval' blob:; "   \
    "worker-src blob:; "                                                                 \
    "style-src 'self' 'unsafe-inline' https://fonts.googleapis.com; "                    \
    "font-src https://fonts.gstatic.com; img-src 'self' data: blob:; media-src 'self' blob:; " \
    "connect-src 'self' https://cdn.jsdelivr.net"
#define CONTENT_SECURITY_POLICY_END "; frame-ancestors 'none'; base-uri 'none'; form-action 'none'"

static const char *TAG = "web_auth";

typedef struct {
    bool used;
    uint8_t key[16];          // peer address (IPv4 in the first 4 bytes)
    uint8_t fails;            // consecutive failures, saturates
    int64_t last_us;
    int64_t locked_until_us;
} lock_entry_t;

// Only touched from the HTTP server task (esp_https_server runs every handler there)
static lock_entry_t s_locks[LOCK_TABLE_SIZE];
static lock_entry_t s_tx_locks[LOCK_TABLE_SIZE];

typedef enum { HDR_ABSENT, HDR_OK, HDR_BAD } hdr_result_t;
typedef enum { AUTH_MISSING, AUTH_BAD, AUTH_OK } auth_result_t;

static void secure_zero(void *p, size_t n)
{
    volatile unsigned char *v = (volatile unsigned char *)p;
    while (n--) {
        *v++ = 0;
    }
}

/** Copy a request header into out. HDR_BAD means it exists but does not fit (never treat that as absent). */
static hdr_result_t read_hdr(httpd_req_t *req, const char *name, char *out, size_t out_size)
{
    size_t len = httpd_req_get_hdr_value_len(req, name);
    if (len == 0) {
        return HDR_ABSENT;
    }
    if (len >= out_size || httpd_req_get_hdr_value_str(req, name, out, out_size) != ESP_OK) {
        return HDR_BAD;
    }
    return HDR_OK;
}

void web_auth_set_common_headers(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");
    httpd_resp_set_hdr(req, "X-Frame-Options", "DENY");
    httpd_resp_set_hdr(req, "Referrer-Policy", "no-referrer");
}

void web_auth_set_html_headers(httpd_req_t *req)
{
    // The HTTPS server sends these pages synchronously in its single handler task.
    // Some browsers do not match WSS against HTTPS 'self'. Permit only this Host's /ws.
    static char websocket_policy[sizeof(CONTENT_SECURITY_POLICY CONTENT_SECURITY_POLICY_END) + HOST_HEADER_BYTES + 10];
    const char *policy = CONTENT_SECURITY_POLICY CONTENT_SECURITY_POLICY_END;
    char host[HOST_HEADER_BYTES];
    if (app_config_tx_enabled() && read_hdr(req, "Host", host, sizeof(host)) == HDR_OK &&
        host[0] != '\0' && strspn(host, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.-:[]") == strlen(host)) {
        const int n = snprintf(websocket_policy, sizeof(websocket_policy),
                               CONTENT_SECURITY_POLICY " wss://%s/ws" CONTENT_SECURITY_POLICY_END, host);
        if (n >= 0 && (size_t)n < sizeof(websocket_policy)) {
            policy = websocket_policy;
        }
    }
    httpd_resp_set_hdr(req, "Content-Security-Policy", policy);
}

static bool reject(httpd_req_t *req, const char *status, const char *body)
{
    httpd_resp_set_status(req, status);
    httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN);
    return false;
}

static void peer_key(httpd_req_t *req, uint8_t key[16], char *label, size_t label_size)
{
    memset(key, 0, 16);
    strlcpy(label, "unknown", label_size);

    int fd = httpd_req_to_sockfd(req);
    struct sockaddr_storage ss;
    socklen_t len = sizeof(ss);
    if (fd < 0 || getpeername(fd, (struct sockaddr *)&ss, &len) != 0) {
        return;
    }
    if (ss.ss_family == AF_INET) {
        const struct sockaddr_in *a = (const struct sockaddr_in *)&ss;
        memcpy(key, &a->sin_addr, 4);
        snprintf(label, label_size, "%u.%u.%u.%u", key[0], key[1], key[2], key[3]);
    } else if (ss.ss_family == AF_INET6) {
        const struct sockaddr_in6 *a = (const struct sockaddr_in6 *)&ss;
        memcpy(key, &a->sin6_addr, 16);
        strlcpy(label, "ipv6", label_size);
    }
}

static lock_entry_t *find_lock(lock_entry_t *locks, const uint8_t key[16], int64_t now)
{
    for (size_t i = 0; i < LOCK_TABLE_SIZE; i++) {
        lock_entry_t *e = &locks[i];
        if (!e->used) {
            continue;
        }
        if (now >= e->locked_until_us && now - e->last_us > FAIL_MEMORY_US) {
            e->used = false;   // quiet for long enough: forget it
            continue;
        }
        if (memcmp(e->key, key, 16) == 0) {
            return e;
        }
    }
    return NULL;
}

static bool is_locked(lock_entry_t *locks, const uint8_t key[16], int64_t now)
{
    const lock_entry_t *e = find_lock(locks, key, now);
    return e != NULL && now < e->locked_until_us;
}

static void record_failure(lock_entry_t *locks, const uint8_t key[16], int64_t now)
{
    lock_entry_t *e = find_lock(locks, key, now);
    if (e == NULL) {
        // Reuse a free slot; otherwise evict the quietest entry that is not locked (or the quietest of all)
        lock_entry_t *victim = NULL;
        for (size_t i = 0; i < LOCK_TABLE_SIZE && victim == NULL; i++) {
            if (!locks[i].used) {
                victim = &locks[i];
            }
        }
        for (size_t i = 0; i < LOCK_TABLE_SIZE && victim == NULL; i++) {
            lock_entry_t *c = &locks[i];
            bool c_locked = now < c->locked_until_us;
            if (!c_locked && (victim == NULL || c->last_us < victim->last_us)) {
                victim = c;
            }
        }
        if (victim == NULL) {
            victim = &locks[0];
            for (size_t i = 1; i < LOCK_TABLE_SIZE; i++) {
                if (locks[i].last_us < victim->last_us) {
                    victim = &locks[i];
                }
            }
        }
        memset(victim, 0, sizeof(*victim));
        victim->used = true;
        memcpy(victim->key, key, 16);
        e = victim;
    }
    if (e->fails < UINT8_MAX) {
        e->fails++;
    }
    e->last_us = now;
    if (e->fails >= LOCK_AFTER_FAILS) {
        // After the first lock-out every further failure locks again, so guessing is limited to
        // one attempt per lock period
        e->locked_until_us = now + LOCK_DURATION_US;
    }
}

static void clear_failures(lock_entry_t *locks, const uint8_t key[16], int64_t now)
{
    lock_entry_t *e = find_lock(locks, key, now);
    if (e != NULL) {
        e->used = false;
    }
}

static bool get_credentials(web_auth_realm_t realm, const app_config_t *cfg, const char **user, const char **pass)
{
    if (realm == WEB_AUTH_REALM_WEB && cfg->web_pass[0] != '\0') {
        *user = cfg->web_user[0] ? cfg->web_user : "admin";
        *pass = cfg->web_pass;
        return true;
    }
    // OTA realm, or the web realm while no web password is set
    *user = cfg->ota_user[0] ? cfg->ota_user : "ota";
    *pass = cfg->ota_pass;
    return cfg->ota_pass[0] != '\0';
}

/** Constant-time check of the Basic authentication header against user:pass */
static auth_result_t check_basic(httpd_req_t *req, const char *user, const char *pass)
{
    char header[AUTH_HEADER_BYTES];
    switch (read_hdr(req, "Authorization", header, sizeof(header))) {
    case HDR_ABSENT:
        return AUTH_MISSING;
    case HDR_BAD:
        return AUTH_BAD;
    default:
        break;
    }

    static const char prefix[] = "Basic ";
    unsigned char given[AUTH_FIELD_BYTES] = {0};
    unsigned char expected[AUTH_FIELD_BYTES] = {0};
    size_t given_len = 0;
    auth_result_t result = AUTH_BAD;

    if (strncasecmp(header, prefix, sizeof(prefix) - 1) == 0) {
        const char *b64 = header + sizeof(prefix) - 1;
        if (mbedtls_base64_decode(given, sizeof(given), &given_len, (const unsigned char *)b64, strlen(b64)) == 0) {
            snprintf((char *)expected, sizeof(expected), "%s:%s", user, pass);
            if (mbedtls_ct_memcmp(given, expected, sizeof(expected)) == 0) {
                result = AUTH_OK;
            }
        }
    }
    secure_zero(header, sizeof(header));
    secure_zero(given, sizeof(given));
    secure_zero(expected, sizeof(expected));
    return result;
}

static bool request_origin_ok(httpd_req_t *req, bool require_origin)
{
    char site[24];
    switch (read_hdr(req, "Sec-Fetch-Site", site, sizeof(site))) {
    case HDR_BAD:
        return false;
    case HDR_OK:
        if (strcmp(site, "same-origin") != 0 && strcmp(site, "none") != 0) {
            return false;
        }
        break;
    default:
        break;
    }

    char origin[ORIGIN_HEADER_BYTES];
    switch (read_hdr(req, "Origin", origin, sizeof(origin))) {
    case HDR_BAD:
        return false;
    case HDR_OK: {
        // The origin must be this very server: https://<Host>
        char host[HOST_HEADER_BYTES];
        if (read_hdr(req, "Host", host, sizeof(host)) != HDR_OK) {
            return false;
        }
        char expected[ORIGIN_HEADER_BYTES];
        int n = snprintf(expected, sizeof(expected), "https://%s", host);
        if (n < 0 || (size_t)n >= sizeof(expected) || strcasecmp(origin, expected) != 0) {
            return false;
        }
        break;
    }
    default:
        return !require_origin;
    }
    return true;
}

/** Cross-site request forgery checks for a request that changes state */
static bool csrf_ok(httpd_req_t *req)
{
    char marker[16];
    if (read_hdr(req, WEB_AUTH_CSRF_HEADER, marker, sizeof(marker)) != HDR_OK ||
        strcmp(marker, WEB_AUTH_CSRF_VALUE) != 0) {
        return false;
    }

    return request_origin_ok(req, false);
}


bool web_auth_check(httpd_req_t *req, web_auth_realm_t realm, bool state_changing)
{
    app_config_t cfg;
    app_config_get(&cfg);

    const char *user = NULL;
    const char *pass = NULL;
    if (!get_credentials(realm, &cfg, &user, &pass)) {
        secure_zero(&cfg, sizeof(cfg));
        web_auth_set_common_headers(req);
        return reject(req, "403 Forbidden", realm == WEB_AUTH_REALM_OTA
                                                ? "OTA is disabled (no password configured)"
                                                : "Access is disabled (no password configured)");
    }

    uint8_t key[16];
    char label[40];
    peer_key(req, key, label, sizeof(label));
    const int64_t now = esp_timer_get_time();

    if (is_locked(s_locks, key, now)) {
        secure_zero(&cfg, sizeof(cfg));
        web_auth_set_common_headers(req);
        httpd_resp_set_hdr(req, "Retry-After", "60");
        httpd_resp_set_hdr(req, "Connection", "close");
        return reject(req, "429 Too Many Requests", "Too many failed attempts");
    }

    const auth_result_t result = check_basic(req, user, pass);
    secure_zero(&cfg, sizeof(cfg));   // user and pass point into cfg: not usable from here on

    if (result != AUTH_OK) {
        if (result == AUTH_BAD) {
            record_failure(s_locks, key, now);
            ESP_LOGW(TAG, "Rejected a request with wrong credentials from %s", label);
        }
        web_auth_set_common_headers(req);
        httpd_resp_set_hdr(req, "WWW-Authenticate",
                           realm == WEB_AUTH_REALM_OTA ? "Basic realm=\"ftx-1 ota\", charset=\"UTF-8\""
                                                       : "Basic realm=\"ftx-1\", charset=\"UTF-8\"");
        return reject(req, "401 Unauthorized", "Unauthorized");
    }
    clear_failures(s_locks, key, now);

    if (state_changing && !csrf_ok(req)) {
        ESP_LOGW(TAG, "Rejected a cross-site or incomplete state changing request from %s", label);
        web_auth_set_common_headers(req);
        httpd_resp_set_hdr(req, "Connection", "close");
        return reject(req, "403 Forbidden", "Forbidden");
    }
    return true;
}

bool web_auth_ws_check(httpd_req_t *req)
{
    char query[16];
    if (!app_config_tx_enabled()) {
        return reject(req, "403 Forbidden", "Remote TX is disabled");
    }
    if (!request_origin_ok(req, true)) {
        ESP_LOGW(TAG, "TX WebSocket rejected: origin check");
        return reject(req, "403 Forbidden", "TX WebSocket origin refused");
    }
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK || strcmp(query, "csrf=ftx1") != 0) {
        ESP_LOGW(TAG, "TX WebSocket rejected: missing or invalid CSRF marker");
        return reject(req, "403 Forbidden", "TX WebSocket CSRF marker refused");
    }
    if (!web_auth_check(req, WEB_AUTH_REALM_WEB, false)) {
        ESP_LOGW(TAG, "TX WebSocket rejected: web login");
        return false;
    }
    return true;
}

bool web_auth_tx_check(httpd_req_t *req, const char *pass)
{
    uint8_t key[16];
    char label[40];
    peer_key(req, key, label, sizeof(label));
    const int64_t now = esp_timer_get_time();
    if (is_locked(s_tx_locks, key, now) || !app_config_tx_enabled()) {
        return false;
    }

    app_config_t cfg;
    app_config_get(&cfg);
    unsigned char given[sizeof(cfg.tx_pass)] = {0};
    unsigned char expected[sizeof(cfg.tx_pass)] = {0};
    strlcpy((char *)given, pass, sizeof(given));
    strlcpy((char *)expected, cfg.tx_pass, sizeof(expected));
    const bool ok = mbedtls_ct_memcmp(given, expected, sizeof(given)) == 0;
    secure_zero(given, sizeof(given));
    secure_zero(expected, sizeof(expected));
    secure_zero(&cfg, sizeof(cfg));
    if (ok) {
        clear_failures(s_tx_locks, key, now);
    } else {
        record_failure(s_tx_locks, key, now);
    }
    return ok;
}

void web_auth_log_startup(void)
{
    app_config_t cfg;
    app_config_get(&cfg);
    if (cfg.web_pass[0] != '\0') {
        ESP_LOGI(TAG, "The web UI is protected by the web credentials (user \"%s\")",
                 cfg.web_user[0] ? cfg.web_user : "admin");
    } else if (cfg.ota_pass[0] != '\0') {
        ESP_LOGW(TAG, "No web password is set: the OTA credentials also protect the web UI. "
                      "Set a dedicated one in the serial menu (options 9 and 0).");
    } else {
        ESP_LOGW(TAG, "No password is set: the web UI refuses every request. "
                      "Set one in the serial menu (options 9 and 0).");
    }
    secure_zero(&cfg, sizeof(cfg));
}
