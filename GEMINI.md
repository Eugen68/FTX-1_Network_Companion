# Instructions for coding agents

ESP-IDF v6.1 firmware for an ESP32-S3 N16R8 that remotes a Yaesu FTDX10 or FTX-1 over Wi-Fi:
USB audio and CAT (CP210x), web UI over HTTPS (`main/index.html`), FT8 decode and transmit,
Hamlib NET rigctl (`main/rigctld.c`), Yaesu CAT over TCP for OmniRig (`main/yaesu_catd.c`),
the FTDX10 ACC scope (`main/acc_frame.c`). Read `README.md` before changing behavior.

## Safety rules (never break these)

1. **Transmit.** Nothing reachable from the network CAT servers (rigctld, Yaesu CAT) may key the
   radio. Do not add `TX1`/`TX2`, PTT or any transmit path to them. Transmit is only allowed from
   the authenticated web TX panel, behind the TX password, the time limit and the heartbeat.
2. **SWR protection, the TX time limit, the heartbeat release and the "TX off" on CAT loss or
   disconnect stay as they are.** Do not weaken, bypass or remove them.
3. **CAT allow-list.** Every CAT command sent to the radio goes through the allow-list in
   `main/cat.c`. Do not add commands that could transmit, change menus (`EX`) or power (`PS`).
4. **Network servers are off by default**, configured from the serial menu, LAN/VPN only.
   Do not open anything to the Internet or add new listening ports without being asked.
5. Never commit passwords, keys, certificates or personal data.
6. License: MIT. Keep the `SPDX-License-Identifier: MIT` line in new files and keep the copyright
   lines in `LICENSE` (pizzamax's line is a condition of his permission).

## How to work

- One small task per branch and pull request. Do not mix unrelated changes. Do not reformat
  files you did not need to change.
- Branch from an up-to-date `main` (`git pull` first). Never push to `main`.
- Keep the existing C style: C11, 4-space indent, `static` for file-local functions, short
  comments that say why. Memory: internal RAM is tight (low watermark ~25–40 KB). Avoid large
  stack buffers and new tasks; prefer PSRAM for big buffers.
- Do not change the version in `main/build_info.h`; the maintainer bumps it.
- Update `README.md` when user-visible behavior changes.

## Build and tests

- Build (ESP-IDF v6.1 shell): `idf.py build`
- Host tests (bash + gcc, e.g. Linux, WSL or the Dev Container): `bash test/<name>/run.sh` for
  `host_cat`, `host_ft8`, `host_tx`, `host_tx_ws`, `host_rigctl`, `host_yaesu_cat`,
  `host_ui_prefs`, `host_acc`, `host_cw`, `host_hn`. Add or extend a host test for new logic.
- If you could not build or run the tests, say so plainly in the pull request. Do not claim a
  check you did not run.

## Pull request

Open it with `gh pr create --base main`. In the description: what changed, why, which files,
which checks you ran and their result, and how to test it on the radio.

## Test firmware for the owner

After a PR builds, make a test firmware: `tools/make_test_zip.sh <next version>` (current version:
see `FTX1_VERSION` in `main/build_info.h`; use the next patch number). It produces
`dist/fw-<version>.zip` with `network_remote_ota.bin`, `ota_data_initial.bin` and
`tools/scrie_firmware.bat`. The owner flashes it on Windows with option 1 (settings kept) and
tests on the radio. Merge only after the owner confirms.

## Where the work stands

- Transmit plan and its phases: `docs/tx-plan.md`. Phases 0–4 are done (PTT, tones, voice, FT8
  transmit). Also done: Hamlib rigctld (Log4OM), Yaesu CAT over TCP for OmniRig (SDRuno,
  CW Skimmer), ACC scope in split with the TX marker, FT8 median DT beside DT corr.
- Phase 5, next, one small PR each: (a) ADIF log of completed FT8 QSOs, stored on the board and
  downloadable from the page; (b) logged QSO sent to Log4OM over UDP, as WSJT-X does;
  (c) assisted FT8 QSO sequence, every transmission still armed by the operator.
- The owner (YO9VA) writes in Romanian: answer in Romanian.
