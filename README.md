| Supported Targets | ESP32-S3 |
| ----------------- | -------- |

# FTDX10 / FTX-1 Network Remote

*Built for hams by a lazy ham*

**English** | [日本語](README.ja.md)

![FTDX10 / FTX-1 Network Remote web interface with VFO controls, S-meter, ACC spectrum and waterfall, and FT8 decoder](docs/network-remote-screenshot.png)

Propagation never sits still, and sometimes you just can't stop wondering what the bands are doing right now. Is 20 m open? Is that station still coming through?
You'd like to know, but getting up and walking to the shack, let alone settling in front of the radio, feels like too much effort.

This is built for exactly those moments. The rig stays where it is. From anywhere in the house, a PC or Mac with a browser is all you need to change frequency and mode, listen to the receive audio, read the S-meter, and even watch FT8 decodes scroll by.
Do it from the sofa, or next to your after-dinner coffee. For the gloriously lazy ham, it is the right tool.

The firmware starts in receive-only mode. Remote push-to-talk is available on the FTX-1 and FTDX10 after it is enabled in the serial menu. See [Transmit](#transmit).

## Based on FTX-1 Network Companion by pizzamax

This project is based on [FTX-1 Network Companion by pizzamax](https://github.com/pizzamax/FTX-1_Network_Companion). It continues as an independent project with the author's permission (see [`docs/PERMISSIONS.md`](docs/PERMISSIONS.md)). See `LICENSE` and `NOTICE` for licensing and attribution.

## Overview

Firmware for the ESP32-S3, a small Wi-Fi-capable microcontroller board. You connect it to the Yaesu FTX-1 over USB, and it relays the rig's receive audio and status over Wi-Fi to the PC / Mac at hand.

- **Audio**: the ESP32-S3 takes the receive audio from the FTX-1's USB audio and streams it to your browser over Wi-Fi. The FTX-1 appears to a computer like a "USB microphone", using the standard USB audio class (UAC 1.0).
- **Control and status**: through the FTX-1's USB serial port, the browser changes frequency and mode and reads the S-meter and other values. This way of controlling and monitoring a radio from a computer is called **CAT** (Computer Aided Transceiver).
- **FT8**: the received audio is decoded inside the browser.
- **CW**: Morse is decoded inside the browser by the DeepCW neural decoder (see "CW decoder").
- **AI noise reduction**: the HamNoise neural denoiser cleans the audio you hear, in the browser (see "AI noise reduction").

All the PC / Mac needs is a browser on the same LAN. No dedicated software is required.

```
FTX-1
 ├─ USB Audio (UAC 1.0) ──────────> ESP32-S3 ──Wi-Fi / HTTPS──> browser / ffplay / VLC
 └─ USB Serial (CP210x / CAT) ────>            ├─ Audio streaming (PCM / WAV)
                                               ├─ Real-time FT8 decoder in the browser
                                               ├─ CW decoder (DeepCW) in the browser
                                               ├─ CAT control (frequency, mode, squelch, ...)
                                               └─ OTA firmware update
```

> "OTA" in the diagram stands for Over-The-Air: updating the firmware over the network, without a cable (see "OTA update").

## Supported radios (this fork)

The firmware recognises the radio from its answer to the CAT command `ID;` and loads a profile for it.

| | Yaesu FTX-1 | Yaesu FTDX10 |
| --- | --- | --- |
| `ID;` answer | `ID0840;` | `ID0761;` |
| CAT rates tried | 115200, 38400, 4800, 9600, 19200 bps (whatever **CAT-1 RATE** is set to) | the same list; the FTDX10 offers up to 38400 (**CAT RATE**) |
| Receivers | MAIN + SUB (two S-meters, AF gain, squelch, notch and FT8 decoders for each) | one receiver; "SUB" is only VFO-B (frequency and mode) |
| Frequency range accepted | 30 kHz – 470 MHz | 30 kHz – 75 MHz |
| Squelch range | 0 – 255 | 0 – 100 |
| BUSY / TX state | `RI0;` | `BY;` and the read form `TX;` |
| S-meter | `RM0;` (MAIN and SUB) | `RM1;` |
| Menu item written in DATA-USB | `EX010411` USB OUT LEVEL = 100 | `EX010413` DATA OUT LEVEL = 100 |
| USB audio (measured) | UAC 1.0, 48 kHz / 44.1 kHz, 2 ch, 16 bit; L = MAIN, R = SUB | UAC 1.0, 18 formats from 8 to 48 kHz, 1 or 2 ch, 8 or 16 bit; 48 kHz 2 ch 16 bit is used |
| USB descriptor | fits the default 256-byte control buffer | larger: needs `CONFIG_USB_HOST_CONTROL_TRANSFER_MAX_SIZE=2048` (set in `sdkconfig.defaults`) |

Both radios connect the same way: one USB cable carries the two CP210x serial ports (the CAT one is found automatically) and the USB audio codec. The browser page adapts to the radio it reports: on the FTDX10 the SUB receiver controls, the SUB scope and the SUB FT8 decoder are hidden, and the 2 m / 70 cm band buttons and the C4FM modes are not shown.

A radio that answers `ID;` with a model the firmware does not know gets a read-only profile: its frequency and mode are shown, but nothing is ever sent to it except read commands.

> The menu numbers differ between the two radios. For example `EX010411` is USB OUT LEVEL on the FTX-1 but HCUT FREQ on the FTDX10, which is why every menu write belongs to a radio profile.

## Hardware requirements

| Item | Requirement |
| --- | --- |
| Board | ESP32-S3 (tested with the **ESP32-S3-DevKitC-1**; see "Recommended board" below) |
| Flash | **16 MB** for the partition table in [`partitions.csv`](partitions.csv) (two 4 MB OTA slots and a data partition). For a smaller flash, see [Changing the flash size](#changing-the-flash-size) |
| PSRAM | Not required. Used when present (for example an **ESP32-S3-N16R8** board with 8 MB octal PSRAM): large buffers (TLS, Wi-Fi) then go to PSRAM and leave internal RAM free |
| Network | A LAN with Wi-Fi (2.4 GHz) |

> If your module has a flash size other than 16 MB, see [Changing the flash size](#changing-the-flash-size) after setting up the development environment.

### Recommended board

This project targets the **ESP32-S3-DevKitC-1**, a development board from Espressif, the maker of the ESP32. It is an entry-level, inexpensive board and is easy to get from online shops such as Amazon. You can start using it with just a USB cable to your PC.

The author uses a compatible board from Freenove (see "How to connect it" for details). Compatible boards with the same layout (two USB ports, a USB-to-UART bridge, an ESP32-S3-WROOM-1 module) should work the same way.

A bare board can short against metal or wiring around the radio, or collect dust. Putting it in a case is recommended if needed. When choosing one, keep the following in mind.

- Both USB ports must stay usable (see "How to connect it" below for what each one is for).
- You must be able to reach the BOOT / RST buttons on the board.
- On many boards the Wi-Fi antenna is on the board itself, so a resin case is the safer choice over a metal one.

### How to connect it

The ESP32-S3-DevKitC-1 has two USB ports. Use them as follows.

| Port | Connect to | When |
| --- | --- | --- |
| **USB-OTG** | **FTX-1** (audio and CAT) | In normal use |
| **USB-UART** | **PC** | Only for the initial setup and flashing |

The label for each port is printed on the board (the silkscreen), next to the port. On the Freenove board the two ports are labeled, and other boards are likely to be labeled in the same way. Check the label before you connect.

- The port that goes to the FTX-1 is USB-OTG. In normal use you do not need to connect a PC to the USB-UART port.
- Connect the USB-UART port to a PC only to use the serial menu (Wi-Fi settings and so on) and to flash the firmware (see "First-time setup and flashing").
- In normal use, the USB-OTG port goes to the FTX-1, so connect a power source to the **USB-UART port**. It does not need to be anything special: an ordinary USB charger or a USB battery pack is enough (the power consumption is small).
- With an FTDX10 you can instead power the board from the radio's ACC socket through a fused, filtered 5 V converter. See [`docs/acc-power.md`](docs/acc-power.md) (a proposal, not yet tested).
- **Use USB cables that carry data.** A charge-only cable does not work for either port.
- **The connector shapes depend on the board.** The official ESP32-S3-DevKitC-1 has two **Micro-USB** ports (see Espressif's [user guide](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.1.html)), while the FTX-1 has a **USB Type-C** port. Choose the cable or adapter to match the connector on your board and on the FTX-1.
- **The author's setup (one example):** a compatible board from Freenove ([FNK0099](https://store.freenove.com/products/fnk0099), with two **USB Type-C** ports) connected to the FTX-1 with a good-quality data cable that has **USB Type-C on both ends** (not a charge-only cable). Because both sides are Type-C, one such cable is enough.
- The ESP32-S3 acts as the USB host (the parent side), and the FTX-1 is the device it reads from.

> **Do not expose it to the Internet.** The firmware takes a reasonable level of security into account, but it is not designed to have a port open to the Internet (for example by port forwarding on your router). Use it only on a trusted LAN at home. See "Security notes" for details.

## How to approach this project

### Who this is for

People who can shrug and say, "Well, that's about what you'd expect." It is not for anyone who needs polish or thorough support.
If it works, lucky you. If it doesn't, have a look inside and fix it. If you can take it in that spirit, we will get along fine.

### Forks are very welcome

It only contains the features I wanted in order to be lazier. Whatever is missing, please feel free to fork it and build more features for the lazy ham.
I would love to see more improvements out there that make life easier.
For technical evaluations and future improvement ideas (such as on-device FT8 decoding directly on the ESP32-S3), see [`docs/future_ideas.md`](docs/future_ideas.md).

### About issues

Issue reports are welcome. Handling them is **best effort**, though: replies, fixes and new features are not promised.
Please be patient, or, even better, fork it and share what you fixed.

## Disclaimer

This project implements only the features that I, a lazy ham, found necessary to make my own life easier. It is not a general-purpose product and does not try to cover every use case.

- The software is provided "AS IS", without warranty of any kind. There is no guarantee that it works on any particular radio, environment or purpose, and no promise of maintenance, bug fixes or support.
- The author accepts no liability whatsoever for any damage arising from the use of, or the inability to use, this software, including damage to radios or peripheral equipment, loss of data, network or security problems, and any other direct or indirect damage.
- Use it entirely at your own risk. Flashing the firmware, OTA updates and connecting equipment are done on your own judgement and responsibility.
- Complying with the laws and regulations that apply to operating a radio station and to receiving radio signals is your own responsibility.
- This is a personal project. It is not affiliated with, or endorsed by, Yaesu Musen Co., Ltd. or any other product or company mentioned here.

## License

The code is released under the [MIT License](LICENSE) (the source files carry `SPDX-License-Identifier: MIT`).
The parts written by pizzamax are distributed under the MIT License with his permission, keeping his copyright notice in `LICENSE` (see [`docs/PERMISSIONS.md`](docs/PERMISSIONS.md)).
When you redistribute it or publish a modified version, keep the `LICENSE` file with its copyright lines.

Part of this project follows the structure of the example code of Espressif's `usb_host_uac` component (Apache License 2.0, the USB Host task in [`main/usb_audio.c`](main/usb_audio.c)), and that part remains under the Apache License 2.0. Details are in [`NOTICE`](NOTICE).
Third-party components fetched at build time (the ESP-IDF and the components under `managed_components`) keep their own licenses. For the projects the FT8 decoder refers to, see "About the FT8 decoder". The CW decoder is a port of DeepCW (MIT License), see "CW decoder"; the AI noise reduction uses HamNoise (MIT License), see "AI noise reduction".

---

## Development environment setup

### Option A: Dev Container (recommended)

A **Dev Container** puts the whole development environment in a Docker container.
ESP-IDF has many installation steps and is awkward to manage in Python, so a Dev Container
sets up "ESP-IDF v6.1 + openssl + the Espressif extension" automatically and gives you the same
build environment on macOS, Linux, and Windows.

**Requirements**:
- [Docker Desktop](https://www.docker.com/products/docker-desktop/)
- [VS Code](https://code.visualstudio.com/) (extension: [Dev Containers](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers))

```bash
# 1. Get the repository
git clone https://github.com/Eugen68/FTDX10_FTX-1_Network_Remote

# 2. Open the folder in VS Code and choose "Reopen in Container"
#    The first run downloads the Docker image (espressif/idf:v6.1), which takes a few minutes
#    ESP-IDF v6.1, openssl, and the Espressif extension are set up automatically
```

Flashing over USB serial needs the `--privileged` flag (already set in `devcontainer.json`).

> **Note**: With Docker Desktop on macOS / Windows, the USB serial port may not be visible from inside the container. In that case, only build inside the container and flash from the PC side (outside the container). The `build/` folder created by the build is also visible from the PC side as is. For the flashing steps, see "Flashing from outside the container" in "First-time setup and flashing".

### Option B: Local (macOS / Linux)

**Requirements**:

| Tool | Version | Where to get it |
| --- | --- | --- |
| ESP-IDF | **v6.1** | [Installation guide](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/get-started/index.html) |
| openssl | any (the system one is fine) | macOS: `brew install openssl` / Ubuntu: `apt install openssl` |
| Python | 3.10 or later (ESP-IDF checks this at startup and stops if it is too old) | macOS: `brew install python` / Ubuntu: `apt install python3 python3-venv` / [python.org](https://www.python.org/downloads/) |
| cmake | 3.22 or later (bundled with ESP-IDF) | Same as above |

ESP-IDF components (`espressif/usb_host_uac`, `espressif/usb_host_cp210x_vcp`, `espressif/mdns`, and others) are
downloaded from the [ESP Component Registry](https://components.espressif.com) during `idf.py build`.

```bash
# 1. Install ESP-IDF v6.1 (first time only)
mkdir -p ~/esp && cd ~/esp
git clone -b v6.1 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf && ./install.sh esp32s3

# 2. Get the repository
git clone https://github.com/Eugen68/FTDX10_FTX-1_Network_Remote
cd FTDX10_FTX-1_Network_Remote

# 3. Activate the ESP-IDF environment
source ~/esp/esp-idf/export.sh        # bash / zsh

# 4. Build (the first build downloads the dependent components)
idf.py set-target esp32s3
idf.py build

# 5. Flash the ESP32-S3 (PORT is /dev/cu.usbserial-* or similar on macOS)
idf.py -p PORT flash monitor
```

> A clean build regenerates the embedded HTTPS fallback. The device certificate is kept in NVS.

## Changing the flash size

The default is 16 MB (`CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y` in [`sdkconfig.defaults`](sdkconfig.defaults)).
If your module is different, change the following.

1. In [`sdkconfig.defaults`](sdkconfig.defaults), change `CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y` to match your module
   (for example `CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y` for 8 MB).

2. Rebuild:

   **Option A (Dev Container)**: run in the VS Code terminal (inside the container)
   ```bash
   idf.py fullclean && idf.py build
   ```

   **Option B (local)**: run in a terminal with the ESP-IDF environment activated
   ```bash
   source ~/esp/esp-idf/export.sh
   idf.py fullclean && idf.py build
   ```

> The partition table is [`partitions.csv`](partitions.csv): two 4 MB app slots and an 8 MB data partition for a 16 MB flash. For an 8 MB flash, make both app slots 0x300000 (3 MB) and shrink or drop `storage`; for 4 MB, go back to two 0x1A9000 slots and drop `storage` (the app must stay below that size).

## First-time setup and flashing

### 1. Build and flash

Credentials (Wi-Fi password and OTA password) do not have to be built in. You can build and flash as is.

Connect **only the USB-UART port** of the ESP32-S3 to the PC, then run the following. Replace `PORT` with the name of the ESP32-S3's serial port as seen from the PC.

```bash
idf.py build
idf.py -p PORT flash monitor
```

| OS | Example `PORT` | How to find it |
| --- | --- | --- |
| macOS | `/dev/cu.usbserial-0001` | `ls /dev/cu.usb*` |
| Linux | `/dev/ttyUSB0` | `ls /dev/ttyUSB*` |
| Windows | `COM3` | "Ports (COM & LPT)" in Device Manager |

- Unplug and replug the cable: the name that disappears and reappears is the ESP32-S3's port.
- If no port appears, you may need the driver for the board's USB-UART chip (CP210x).
- To leave `idf.py monitor`, press `Ctrl` + `]`.

#### Flashing on Windows with a double click

[`tools/scrie_firmware.bat`](tools/scrie_firmware.bat) (Romanian prompts) installs esptool if needed, lists the COM ports, and either updates the app while **keeping** the Wi-Fi and passwords (`ota_data_initial.bin` at 0xf000 and the app at 0x20000) or writes the whole merged image (which **erases** the settings, for a new board). Put it in the folder with the `.bin` files from `build/`, with `network_remote-merged.bin` from the merge command below and a copy of `network_remote.bin` named `network_remote_ota.bin`.

> Writing the merged image at 0x0 also overwrites the NVS partition (0x9000), so the settings are lost. To update a configured board, write only the app, or use OTA.

#### Flashing from outside the container

If the USB serial port is not visible from inside the Dev Container, build inside the container and flash only from the PC side.

```bash
# Inside the container (build, and create the file used for flashing)
idf.py build
idf.py merge-bin -o network_remote-merged.bin  # creates build/network_remote-merged.bin

# On the PC side (outside the container). Install esptool once, then flash
python3 -m pip install esptool
python3 -m esptool --chip esp32s3 -p PORT write-flash 0x0 build/network_remote-merged.bin
```

After flashing, open the ESP32-S3's port with any serial monitor you like, such as Tera Term (115200 bps).

### 2. Interactive setup over the USB UART serial console (stored in NVS)

Settings such as the Wi-Fi network and passwords are stored in the ESP32-S3's **NVS** (Non-Volatile Storage: an area inside the microcontroller that keeps its contents when the power is off).
After flashing, or at boot, connect a PC to the USB-UART port and open a serial monitor for the ESP32-S3 (`idf.py monitor`, Tera Term, and so on).

- **First boot (Wi-Fi not configured)**: the configuration menu opens automatically.
- **Already configured**: a 3-second countdown is shown at boot. Press `c`, `m`, or `Enter` to enter the configuration menu. You may also press the BOOT button, but **only after the board has started**. If you hold it down during a reset or power-up, the board starts in flashing mode instead of running the firmware, and the menu does not appear.

```text
**************************************************
*       FTDX10 / FTX-1 Network Remote Configuration Menu       *
**************************************************

==================================================
           CURRENT RUNTIME CONFIGURATION
==================================================
 1) Wi-Fi SSID       : (not set)
 2) Wi-Fi Password   : (empty / disabled)
 3) OTA User ID      : ota
 4) OTA Password     : (empty / disabled)
 9) Web User ID      : admin
 0) Web Password     : (empty / disabled)
 h) Host name        : trx-remote.local
==================================================
Options:
  1: Set Wi-Fi SSID
  2: Set Wi-Fi Password
  3: Set OTA User ID
  4: Set OTA Password
  9: Set Web User ID
  0: Set Web Password
  h: Set Host name (mDNS / DHCP)
  5: Show passwords in plain text
  6: Save to NVS and Restart device
  7: Exit and continue boot without saving
  8: Erase NVS configuration (Reset to defaults)
--------------------------------------------------
Enter selection (0-9, h):
```

Set at least these three.

1. Enter `1`, then the Wi-Fi SSID.
2. Enter `2`, then the Wi-Fi password (shown masked).
3. Enter `0`, then the **web password** (at least 12 characters; this is the password you use to log in from the browser).

Then set the following as needed.

- `9`: the web user ID (default `admin`)
- `3` / `4`: the OTA update user ID and password (the password must be at least 12 characters)
- `h`: the host name (the default is `trx-remote`; see "Host name (mDNS)" below)

Finally, enter `6` to run "Save to NVS and Restart device". **Nothing takes effect until you save with `6`.**

> **The web password is required.** Without it, the browser shows `Access is disabled (no password configured)` (HTTP 403) and nothing opens. It never opens without authentication.
>
> The SSID and passwords accept only half-width alphanumerics and symbols (ASCII). Non-ASCII characters, such as Japanese, cannot be entered.

> **Benefit when publishing on Git**: all passwords are stored in the microcontroller's NVS (non-volatile memory). No private settings remain in the Git repository or in build artifacts, so you can publish the source or share binaries safely.

### 3. Check it in the browser

1. After the restart the device connects to Wi-Fi, and the serial monitor log shows `Got IP: x.x.x.x` and `Open https://trx-remote.local/`.
2. Turn on the FTX-1 and connect the ESP32-S3's **USB-OTG port** to a USB port on the FTX-1 (see "How to connect it").
3. In a browser on a PC / Mac on the same LAN, open `https://trx-remote.local/` (or `https://<the IP shown in the log>/`). Use `https://`, not `http://`.
4. Because the certificate is self-signed, **a warning appears**. This is your own device on your own LAN, so check the details and continue (accept) (see "Self-signed certificate").
5. You are asked for a user ID and password. The ID is `admin` (or the ID you set with `9`), and the password is the web password you set with `0`.
6. Press **POWER / PLAY** to start playing the received audio. The button changes to `PWR OFF / STOP`; press the same button again to stop.

If it does not work, see "Troubleshooting".

## Host name (mDNS)

While no host name is stored in NVS (first boot, or after erasing the configuration with `8`), the host name is `trx-remote`.
It is reachable at `https://trx-remote.local/` through **mDNS** (Multicast DNS: a way to reach a device on the same LAN as "name.local" without a DNS server; using [`espressif/mdns`](https://components.espressif.com/components/espressif/mdns)).
The same name is also sent as the DHCP host name (the name shown in your router's client list).
On upgrade, a stored `ftx-1c` factory default is used as `trx-remote` in memory. Custom host names
are kept.

### Changing the host name (serial menu)

1. Open the menu in the serial monitor (automatic when Wi-Fi is not configured; otherwise press `c` / `m` / `Enter` within 3 seconds after boot).
2. Enter `h`, then the host name **without** `.local`. Press Enter alone to keep the current value.
3. Enter `6` (Save to NVS and Restart device). **The change takes effect only after saving and restarting.** Leaving with `7` discards it.

```text
Enter selection (0-9, h): h
Enter Host name without ".local" [Current: trx-remote] (a-z, 0-9, '-'; DEFAULT to reset; Enter to keep): radio-shack
-> Host name set to: radio-shack.local
```

- Allowed: lowercase `a-z`, `0-9` and `-`; no `-` at the start or end; up to 63 characters. Names with uppercase letters, `_`, spaces or `.` are rejected ("Rejected" is printed) and the old value is kept.
- Enter `DEFAULT` to go back to `trx-remote` (option `8`, "Erase NVS configuration", also resets it together with the other settings).
- If the stored value is invalid for any reason, it is replaced by `trx-remote` at boot.
- The URL is also printed in the boot log as `Open https://<host name>.local/`.
- Update bookmarks and commands (`ffplay`, `curl`, and so on) to the new name. The old name no longer works.

### Notes

- **Certificate names**: a newly generated device certificate covers `trx-remote.local`, `trx-remote` and the board’s IPv4 address at generation time. A custom host name can show a name-mismatch warning.
- **Duplicate names**: if another device with the same name is on the LAN, mDNS may automatically rename this one, for example `trx-remote-2.local`. When you use several units, give each a different host name.
- **Browser state**: a new host name is a different site for the browser, so you have to log in (Basic authentication) and accept the certificate again.
- macOS, iOS, Windows 10 and later, and recent Linux (avahi) resolve `.local` out of the box.
- Where it does not resolve, use the IP address shown in the log.

## Usage

### Split

**SPLIT** (beside SWAP and A→B) turns split on or off: receive on MAIN, transmit on SUB. It
lights up whenever the radio transmits on SUB, also when split was set on the radio itself.
**SPLIT +1k** copies MAIN to SUB, puts SUB 1 kHz up and turns split on. Neither button keys the
radio, and the change is refused while the radio transmits.

### Front end: pre-amp, attenuator, RF gain, AGC

Below the notch, each receiver has a **FRONT END** row read from the radio every 5 s and after each change:

- **AMP** (MAIN only): IPO / AMP1 / AMP2. On the FTX-1 the CAT pre-amp command is per band and follows MAIN's frequency: IPO / AMP1 / AMP2 on HF and 50 MHz, OFF / ON on VHF and on UHF. The FTX-1 keeps a separate IPO for SUB, but CAT reaches only MAIN's, so set SUB's on the radio.
- **ATT** (MAIN only): OFF / 6 / 12 / 18 dB on the FTDX10, OFF / ON on the FTX-1.
- **RF GAIN**: 0–255.
- **AGC**: OFF / FAST / MID / SLOW / AUTO. In AUTO the radio reports which speed it chose (AUTO-FAST / -MID / -SLOW).

On the FTX-1, RF GAIN and AGC are separate for MAIN and SUB; the FTDX10 has one receiver. These are receive settings only.

### Noise, width and IF shift

Under FRONT END, each receiver has two more boxes, read the same way:

**NOISE / DNR / DNF**, on one row:

- **NB** (noise blanker): OFF or level 1–10 (0–10 on the FTDX10, which has a separate ON/OFF).
- **DNR**: OFF or level 1–15 on the FTDX10, 1–10 on the FTX-1.
- **DNF** (auto notch): ON / OFF.

**WIDTH / IF SHIFT**, one slider under the other:

- **ROOF** (FTDX10 only, at the top; the FTX-1 has no roofing filter command): the roofing filter, 12 kHz,
  3 kHz, 500 Hz or 300 Hz (CAT `RF`). 300 Hz needs the optional filter; the list always shows what the radio reports.
- **WIDTH**: the receiver filter width (CAT `SH`), in the steps of the radio's bandwidth chart for the mode,
  shown in Hz. SSB has 23 steps from 300 Hz to 4 kHz; CW, RTTY, DATA and PSK 21 steps from 50 Hz to 4 kHz.
  AM and FM have fixed widths: the slider is greyed out and shows `fixed`. `default` means the radio uses
  the mode's default width (on the FTDX10 it depends on the roofing filter).
- **IF SHIFT**: −1200 to +1200 Hz in 20 Hz steps. Double-click the slider or press **0** to centre it.

All are read from the radio every 5 s and right after a change, so a change made on the radio shows up too.
On the FTX-1 they are separate for MAIN and SUB; on the FTDX10 they apply to its one receiver (the width is
the active VFO's).

### FTDX10: A / B (one receiver)

The FTDX10 has one receiver and two VFOs, and its **A/B** key selects the VFO it receives and transmits on
(CAT `VS`). The page works the same way:

- The page's **A / B** button (where the FTX-1 has SWAP) swaps VFO-A and VFO-B (CAT `SV`), so you hear the
  other frequency at once and VFO-A, which remote PTT uses, takes it. (CAT `VS` would move the display to
  VFO-B but leave the receiver on VFO-A's frequency.) The page also follows the radio's own **A/B** key.
- The left panel always shows and changes the **active** VFO: band, mode, dial, arrows, step, ZIN and direct entry.
  It turns green and reads **VFO-B (A/B)** while VFO-B is active. The screen keeps VFO-A above and VFO-B below,
  with the active one framed in green. There is no separate VFO-B card on the FTDX10: the right column is
  **TX & RX SETTINGS** instead, with the TX panel and, under it, the FRONT END, NOISE / DNR / DNF and WIDTH / IF SHIFT boxes.
- FT8, the scope and recordings follow the active VFO.
- **Remote PTT (and FT8 transmit) works on VFO-A only**: with VFO-B active the TX panel reports
  "Select VFO-A (A/B) for remote PTT", and VFO-B becoming active while transmitting releases PTT.
- The FTDX10 reports MD0 / FT for the active VFO (MD1 for the other); the board maps them to VFO-A / VFO-B
  for the page. NET rigctl (Log4OM) is unchanged and still assumes VFO-A is active.

The FTX-1, with two receivers, keeps its MAIN and SUB panels and SWAP.

### Endpoints

All of them require Basic authentication (see "Security notes" below).

| Path | Description |
| --- | --- |
| `/` | Browser player |
| `/status` | Stream state and diagnostics (JSON): firmware version (`version`), build time (`build`), audio format, uptime, heap (`heap_free` / `heap_min` / `heap_block`; these include PSRAM if it is enabled), internal-RAM-only heap (`int_free` / `int_min` / `int_block`), Wi-Fi `rssi`, stream send statistics (`stream.slow_sends` / `stream.max_send_ms`, and others), the CAT commands the radio refused: `cat_rejected_commands` (count since boot), `cat_last_rejected` (the last one, e.g. `"BP00;"`) and `cat_last_rejected_age_s` (seconds ago, `null` if none), and the last USB audio problems: `stream.usb_last_loss` (the radio's USB audio input stopped: `device disconnected`, `RX restore failed` or `RX pause failed`) and `stream.usb_last_reset` (the board power-cycled the USB port: `no audio input` or `CAT ID missing`), each with `age_s` and the internal RAM at that moment (`int_free`, `int_min`), `null` if it did not happen since boot |
| `POST /cat` | Controls the radio (frequency, mode, volume, and so on; JSON, `X-Requested-With: ftx1` required) |
| `POST /reboot` | Reboot the ESP32-S3 (JSON, `X-Requested-With: ftx1` required) |
| `/stream.pcm` | Raw PCM (little endian). The format is reported in the `X-Sample-Rate` / `X-Channels` / `X-Bytes-Per-Sample` headers |
| `/stream.wav` | With a WAV header (indefinite length). For external players |
| `POST /ota` | Firmware update (Basic authentication; see "OTA update" below) |
| `http://…` (port 80) | Redirects every request to the same host over `https://` (so typing just the IP or `trx-remote.local` works). Serves nothing else and asks for no login |

Example with an external player (48 kHz / 16 bit / mono):

```
ffplay -nodisp -tls_verify 0 https://trx-remote.local/stream.wav
```

### Log4OM and WSJT-X over Wi-Fi (Hamlib NET rigctl)

In the USB serial menu, use **n** to enable NET rigctl, **r** to change its TCP port
(default **4532**) and **i** to optionally allow a single client IPv4 address. Use **6** to
save and restart. The server is off by default and accepts up to two clients.

- **Log4OM** — open the CAT settings, choose **Hamlib** as the CAT engine and
  **NET rigctl** as the rig model. Set the connection to the board's IPv4 address
  and TCP port, for example `192.168.55.138:4532`.
- **WSJT-X** — go to **Settings → Radio**, set **Rig** to **Hamlib NET rigctl**
  and **Network Server** to the board's `IP:4532`. For **PTT Method** use
  **None** if offered, or **VOX** with the radio's VOX disabled for receive-only
  use. **Test CAT** verifies the connection. NET rigctl cannot key or unkey the radio.
- **Hamlib CLI** — run `rigctl -m 2 -r 192.168.55.138:4532 -vvvv f m v s t get_powerstat`.
  The authenticated HTTPS `/status` endpoint includes the `rigctld` enable/listen state,
  port and client, command and rejection counts.

Rejected requests appear in the serial log with the client IPv4 address, request and
response. The latest rejection is available as `rigctld.last_rejected` in HTTPS
`/status`. To trace successful requests as well, enable **d** in the serial menu and
save with **6**. The 1.9.42 Log4OM test firmware starts with tracing enabled. Use **d**, then **6**, to turn it off after the test.

If a client stops reading and a reply cannot be queued within one second, the
server closes that connection. TCP keepalive detects lost idle peers. HTTPS `/status` reports the current lwIP socket
descriptor count and limit under `lwip`. The `rigctld` section reports its own
listener and client socket count, admitted connections opened and closed, and
accept/send errors and send timeouts. The counts are snapshots of open socket
descriptors, so closed TCP connections in TIME_WAIT are outside the count.

Frequency and mode setters accept an optional VFO before their values, for example
`F VFOA 7025000` and `M VFOA CW -1`. They update that side directly while the active
VFO stays selected. A mode width of `-1` or `0` leaves the radio to handle its filter
without a separate CAT filter command. An explicit split request such as
`S VFOB 1 VFOA` selects VFO-B for receive and VFO-A for transmit.

NET rigctl is plain TCP with no password. Use it only on a trusted local network
or over a VPN. Keep the port off the public Internet. Clients can change frequency,
mode, VFO and split only while CAT confirms reception. The service rejects PTT
commands even when transmit through the web interface is enabled. Frequencies use
whole Hz. Mode sets accept an available filter width, `0` for normal or `-1` to leave
the width alone. For an adjustable filter, a reported width of `0` means the
station returned its default code or the width has yet to be read. All getters use
cached CAT state. When this feature is enabled, the existing CAT refresh also reads
the active VFO, transmit VFO and filter settings one command at a time.

### OmniRig (SDRuno, CW Skimmer …) over Wi-Fi (Yaesu CAT over TCP)

OmniRig talks only to COM ports. The board can act as the radio on a TCP port, and a virtual
COM port on the PC connects OmniRig to it:

```
Program → OmniRig (FTDX10 or FTX-1 profile) → COM10 (virtual) → Wi-Fi → board, TCP 4533 → radio
```

1. In the USB serial menu, use **y** to enable the Yaesu CAT server, **u** to change its TCP
   port (default **4533**) and **i** for the optional allowed client IPv4 address (shared with
   NET rigctl). Save with **6**. The server is off by default and accepts up to two clients.
2. On the PC, create a virtual COM port mapped to TCP, for example with
   **HW Virtual Serial Port** (HW group): a new port such as **COM10**, connected to the board's
   IPv4 address and port `4533`. com0com with hub4com works as well.
3. In **OmniRig**, set Rig 1 to the radio's profile (FTDX10 or FTX-1) on **COM10**. The baud rate
   setting is not used on a virtual port, but choose one (for example 38400). Set **DTR** and
   **RTS** to **Low**, **Poll interval 1000 ms** and **Timeout 4000 ms**. With the default 500 ms,
   a cycle of eight commands through the virtual COM port and Wi-Fi does not always fit, and
   OmniRig (and CW Skimmer) flip between on-line and off-line. Set this on every rig in OmniRig
   that uses the board.
4. Point the programs at OmniRig Rig 1. Several programs can share it, for example **SDRuno**
   following the radio's frequency and **CW Skimmer** reading it and sending click-to-QSY.

How the board answers:

- Reads (`ID;`, `FA;`, `FB;`, `IF;`, `OI;`, `MD0;`, `MD1;`, `FT;`, `ST;`, `VS;`, `TX;`, `KP;`, `AI;`, `PS;`,
  `SM0;`, `SH0;` …) are answered from the CAT state the board already keeps, in the connected
  radio's exact format (`IF;` is 28 bytes on the FTDX10 and 30 on the FTX-1). They add no traffic
  on the radio's CAT port. The CW pitch (`KP;`) is read by the board every 5 s anyway (the page's CW
  decoder uses it, see RIG), whether the server is enabled or not. Unknown reads get `?;`, as from the radio.
- OmniRig's FTDX10 profile polls `VS;`, `FT;`, `TX;`, `MD0;`, `IF;`, `FA;`, `FB;` and `KP;` in
  every cycle. A wrong reply length there makes OmniRig (and CW Skimmer) flip between on-line
  and off-line.
- Writes for frequency, mode, active VFO, transmit VFO, split on / off (`ST1;` / `ST0;`, as
  OmniRig sends them) and VFO swap or copy go through the same
  allow-list as the web page. They are refused while transmitting. Like the radio, the board
  never answers a write, so programs that read fixed-length replies stay in step.
- Fast frequency changes (a program following a tuning knob) are combined: at most one per VFO
  every 100 ms reaches the radio, always the latest. Reads report the requested frequency until
  the radio confirms it.
- `TX1;` and `TX2;` are refused, as are power, menu, memory and keyer writes. `TX0;` is accepted
  without effect. `AI1;` from a program is kept for that program: the board sends it `FA`, `FB`,
  `MD0` and `MD1` changes, while the radio's own auto information stays on for the board.
- While no radio answers CAT (off or still starting), the board does not answer at all, so
  OmniRig shows the radio as offline.

HTTPS `/status` reports the server under `yaesu_cat`: clients, commands, writes, rejections and
the latest rejected command. Use **g** in the serial menu to trace every command and reply.

Like NET rigctl, this is plain TCP with no password: use it only on a trusted local network or
over a VPN.

## The FTDX10's own scope from the ACC socket

With an FTDX10, the page can show the radio's own spectrum and waterfall (up to a 1 MHz span) instead of the audio
scope. The radio clocks its scope out of the ACC socket as SPI, 25 frames a second; this board reads it directly.
The frame layout is the one measured on an FTDX10 for the [Yaesu ACC Spectrum](https://github.com/mm5agm/Yaesu_ACC_Spectrum)
project (`docs/protocol.md`); the code here is written from that description.

**Wiring** (the same as that project's board): each signal through **470 ohm**, ground direct.

| ACC pin | Signal | GPIO |
|---|---|---|
| 2 | GND | GND |
| 3 | DATA1 | 14 (unused) |
| 4 | DATA2 | 11 |
| 5 | CLOCK | 10 |
| 6 | CS | 13 |
| 11 | CNT RX | 39 |
| 12 | CNT TX | 38 |

Link GPIO38 and GPIO39 (the free ends of the pin 11 and 12 resistors) with a wire: the radio's once-a-second
`alive` then comes back and the radio streams. **Never connect ACC pins 1, 7, 8 (13.8 V), 9 (PTT), 10 and 13
to the board.** Pin 8 may only feed the optional fused 5 V supply described in [`docs/acc-power.md`](docs/acc-power.md).
Plug and unplug only with the radio's power supply off. Read that project's `docs/fuse-risk.md` first.

- The pins are set up only after CAT has identified an **FTDX10**; with any other radio nothing is done with them.
- Each frame is checked (length, FTDX10 layout, both VFO copies, span and placement, and the VFO against CAT's
  frequency); `/status` → `acc` shows frames per second, accepted / rejected per reason.
- `GET /scope.bin` streams the rows (`?fps=1-25`). In the page, the MAIN scope has a **waterfall source** selector:
  **Auto** (the ACC scope when its data arrives, otherwise the audio scope), **Audio**, **ACC**. Click or tap on the
  ACC waterfall to tune MAIN there. The display scale follows the noise floor and the strongest signals.
- While the ACC scope is shown, a **SPAN** selector (1 kHz – 1 MHz) in the MAIN scope header sets the radio's scope
  span over CAT (`SS05…`); it shows the span the radio reports in its scope data, so it also follows the radio's own knob.
  Beside it, a **LEVEL** slider (−30 to +30 dB in 0.5 dB steps) sets the radio's scope LEVEL (`SS04…`) and shows
  the radio's value; about +20 suits the page. Placement (Center / Cursor / Fix) is set on the radio.

## Pseudo squelch (in the browser)

The USB audio of the Yaesu FTX-1 may ignore the radio's own squelch (noise comes out even with no signal).
So the player page ([`main/index.html`](main/index.html)) has a pseudo squelch that judges the audio and mutes it.
**It affects only playback in this page; `/stream.pcm` and `/stream.wav` are unchanged.**

### Detection modes

| Mode | Opens when | Use |
| --- | --- | --- |
| Variation (default) | The level swing (max − min) over the last 300 ms exceeds the "variation threshold" (default 8 dB) | AM |
| Level | The level exceeds the "threshold" (default -34 dBFS). "Invert" reverses it | FM / SSB |

- With no signal, AM noise settles at about -27 dBFS because of AGC, and the level can be about the same while a signal is received.
  The level cannot tell them apart, so AM is judged by the swing.
  In measurements, 99% of the 300 ms swings with no signal were 6.4 dB or less (max 7.2 dB), and 10 to 40 dB while receiving.
- After opening it holds for 0.4 s so word endings are not cut. Hysteresis is 3 dB (level mode) and the fade is 10 ms.
- Settings (on/off, mode, thresholds, which audio to judge) are stored in the browser's localStorage.

### L and R are judged separately

The FTX-1 USB audio is 48 kHz stereo: L is MAIN (VFO-A) and R is SUB (VFO-B). This does not change with the frequency band.
By default ("judge L and R separately") each channel has its own gate, so when only one side receives, the other stays closed.
Below the meter, the level, swing, and open/closed state of each channel are shown. If it misbehaves, adjust the thresholds using those numbers.

- Raise the variation threshold: when it opens by mistake with no signal (about 10 to 12 dB)
- Lower the variation threshold: when it does not open while receiving (about 6 dB)
- Choosing something like "judge on R, open and close both" makes one gate open and close both channels together.

#### Safe CAT startup

CAT waits at least three seconds after USB enumeration or USB bus recovery. It probes each
supported baud rate once per scan, starting with the last rate saved in NVS. Only an
`ID` reply with four digits and a semicolon selects a CAT port. Initial reads go out one at a time.
Startup writes, including TX0 and the DATA USB output level, wait until those reads finish.
Automatic information is enabled when `AI;` reports it off or the radio gives no reply after
a successful write. The serial log reports the duration of the initial reads and the first
`sync_all` refresh on each CAT connection.

The authenticated `/status` counter `cat_rejected_commands` counts radio `?;` replies. An explicitly
rejected periodic read is skipped for that USB connection. The CAT log records the command and
its rejection.

## Limitations

- A continuous voice that stays at almost the same level for 300 ms or more can cause it to close (eased by the 0.4 s hold).
- It does not judge by the content of the audio, so noise with large swings (such as lightning) opens it.
- Reading the radio's BUSY or S-meter over CAT would be more accurate, but it needs USB serial (CDC) host support and is not implemented.

## Decoder selector

Under the scopes there is one decoder panel per receiver: MAIN, and SUB on the FTX-1. The **DECODER** list in
its header chooses what runs on that receiver: `OFF` (the default), `FT8` or `CW`. Choosing a decoder starts it,
`OFF` stops it. MAIN and SUB are independent (for example CW on MAIN and FT8 on SUB), and only one decoder runs
per receiver. The panel shows only the parts of the chosen decoder.

- The controls shared by MAIN and SUB stay in the MAIN header and show while either receiver uses that
  decoder: DT corr, med and APPLY for FT8; Model, AUTO, Hz and the status for CW.
- The FT8 transmit controls (shown after TX authorization) do not depend on the list and always stay visible,
  so `Halt` is reachable whatever decoder is chosen.
- FT8 keeps its choice on the board, as before; CW keeps it in this browser. If the board says FT8 for a
  receiver, CW stops there.

## About the FT8 decoder

The real-time FT8 decoder implemented in the browser ([`main/index.html`](main/index.html)) is based on the specifications and implementations of the following open-source projects.

| Project | What was referred to |
| --- | --- |
| **[ft8_lib](https://github.com/kgoba/ft8_lib)** by Karlis Goba (YL3JG) — MIT License | LDPC(174, 91) parity-check matrix, CRC-14 polynomial, 8-FSK Gray mapping, message unpacking specification |
| **[WSJT-X](https://wsjt.sourceforge.io/)** by Joe Taylor (K1JT) et al. — GPLv3 | The FT8 protocol specification in general (Costas array, 77-bit message format, LDPC code parameters) |

It is a full-stack FT8 decoder written only in JavaScript for the browser, using the projects above as reference.
To the best of the author's knowledge the code of those projects was not ported or copied directly,
but the protocol constants (parity-check matrix, Gray code table, CRC polynomial, Costas sequence, and so on)
are the specification values published by WSJT-X / ft8_lib, used as they are.

This project would not exist without the contributions of those who came before. We are deeply grateful to all the developers.

### Decoding performance settings

- Up to the top 200 sync candidates are tried, over an audio range of about 125 to 3000 Hz (assuming a crowded 40 m slot).
- Candidates whose Costas match ratio is below 3.5 are not decoded (to suppress false detections).
- Decoding runs once per slot in a Web Worker (a separate thread). A crowded slot takes 1.5 to 1.8 seconds on a Mac, but audio playback and the UI do not stop. The Worker is created from a Blob URL, so the CSP allows `worker-src blob:`.
- The second pass (subtracting decoded signals and searching again) is not implemented. For that reason it cannot pick up weak stations hidden behind strong ones as well as WSJT-X or ft8_lib.

### Time synchronization (NTP) and latency considerations

Due to Wi-Fi transmission and browser audio buffering, an unavoidable transmission latency (typically 100 to 300 ms) exists between when the transceiver receives audio and when it arrives at the browser. The system is designed to account for this:

- **Absorption by DT (Time Offset) search**: The FT8 protocol is inherently designed to tolerate propagation delays and clock skews between stations. The decoder searches for timing offsets (DT) during sync detection (Costas array search). The network latency is naturally absorbed as a positive shift in DT (typically +0.1s to +0.4s) and does not affect decoding success.
- **Adjusted decode trigger timing (14.0s)**: In a standard 15-second FT8 slot, transmission takes place between 0.5s and 13.14s (for DT=0). To accommodate transmission delay and buffering of late-arriving symbols, the decoder snapshots 15.0 seconds of audio and triggers the Web Worker at **14.0s** into the cycle rather than the 15.0s slot boundary. This ensures trailing symbols are not cut off.
- **DT corr and its median**: the **DT corr** field in the MAIN decoder header is subtracted from every DT shown, so DTs read as in WSJT-X. Beside it, **med** shows the median raw DT of the stations decoded on MAIN and SUB in the last four slots, and how many (at least five are needed). **APPLY** copies it, rounded to 0.1 s, into DT corr; nothing is applied on its own. It only changes the DTs shown, not decoding or transmit timing.
- **Waveform continuity**: Network jitter in packet arrival does not alter the sample rate timing; audio is placed into the continuous ring buffer at fixed sample rates (48 kHz / 6.4 kHz), preserving the exact relative phase and timing across symbols.

### FT8 world map

![FT8 world map window: stations plotted by grid locator, with the list of decoded stations below (callsigns in the screenshot are dummies)](docs/ft8_map.png)

Choosing `FT8` in a DECODER list opens a world map in a separate window (`/map`) and plots each station at the grid locator (4 characters) contained in the decoded message.

- Call signs are not always shown on the map. Click a marker to see it together with the grid, SNR, distance, and bearing. You can also select a station by clicking a row in the list, and a station that is off screen is moved into view. When several stations overlap at the same grid, each click cycles through them in turn. Click an empty spot or press Esc to deselect.
- The map can be zoomed and dragged (up to ×40). Zoom with the mouse wheel, a double click, or `+` / `−` in the header, and return to the whole view with `FIT`. When zoomed in, grid squares (such as `PM95`) and their names appear.
- Enter your own grid (4 or 6 characters) in `My grid` in the header and press `SET` to enable the distance and bearing display and the great-circle lines. The value is stored in the browser's `localStorage`.
- Station colors are by band (160 m to 6 m), not by MAIN / SUB. The legend lists only the bands currently shown. QSO lines are amber and your own station is red. The list has a BAND column.
- Only messages that contain a grid (CQ and first calls) are plotted. Report exchanges, `RR73`, `73`, and hashed call signs are not.
- Old spots fade and disappear after 15 minutes. Closing and reopening the map window restores the spots so far.
- The coastline is the [Natural Earth](https://www.naturalearthdata.com/) (public domain) 50m land data, thinned at 0.02 degrees and embedded as vectors in [`main/map.html`](main/map.html) (about 220 KB). The outline stays smooth when zoomed in, and it works without an Internet connection.
- If the pop-up is blocked, allow pop-ups for this site from the blocked indicator in the address bar.

## CW decoder

Choose `CW` in the DECODER list of a receiver (see "Decoder selector"). The CW decoder is a port of
[DeepCW](https://github.com/e04/web-deep-cw-decoder) by e04 (MIT License): a neural network trained on
weak, fading and noisy CW, which copies well below the level where classic decoders give up. It only
listens: it never keys the radio and sends no CAT command.

- Text appears word by word; the last word, still uncertain, is shown in grey
  until it is confirmed. A new station or the end of a transmission starts a new line. `CLEAR` empties the panel.
- **Model**: `Narrow` (default) decodes one signal, the tone at the pitch; `Wide` decodes the strongest
  signal between 400 and 1200 Hz, is heavier and needs more CPU (a phone may struggle).
- **Stream**: how the text is confirmed (the same for MAIN and SUB, kept in this browser).
  - `Pauses` (default): every second the decoder re-decodes all the audio not confirmed yet (up to 30 s)
    and confirms the text at the gaps between words.
  - `Rapid` / `Normal` / `Precise`: overlapping windows. A fixed 12 s window is decoded every 2 / 4 / 8 s,
    each piece of audio only once. Where two windows overlap their characters are matched in time and the
    more confident copy is kept, so a character cut by the edge of one window is read from its neighbour.
    The confirmed text is final.

    | Stream | Window every | Text confirmed after (median) |
    |---|---|---|
    | Rapid | 2 s | about 4 s (on weak, noisy signals it may miss a character more) |
    | Normal | 4 s | about 7 s |
    | Precise | 8 s | about 13 s |

    The grey text (the words not confirmed yet) is refreshed every 2 s, from just before the confirmed
    part to the end of the audio. The last word of a transmission is confirmed about 3 s after the signal stops.
    Changing between Rapid, Normal and Precise applies from the next window without losing text; changing
    between Pauses and the windows confirms the pending text and starts afresh.
- **AUTO** (narrow model): follows the strongest CW tone between 300 and 1200 Hz and shows it next to the title.
  With AUTO off, the tone in **Hz** is decoded, normally the radio's CW pitch (700 Hz by default). Narrow the
  radio's filter around the station for the best copy.
- **RIG** (on by default, narrow model with AUTO off): the Hz field follows the radio's own CW pitch, which the
  board reads from the radio (`KP`) every 5 s, so a correctly tuned station is decoded without typing its tone.
  The field then shows the radio's pitch, greyed out; a pitch change on the radio reaches the decoder within
  about 5 s, and the text still pending is kept. Until the pitch has been read, or with RIG off, the manual Hz
  value is used. A tone chosen on the scope still wins over RIG.
- **Click a signal on the audio scope** of a receiver that uses CW: that receiver decodes the CW signal at that
  audio frequency (with the narrow model), whatever AUTO or Hz say. A cyan band marked `CW 720Hz` shows on the
  scope where the decoder listens (dashed while AUTO follows a tone, solid when chosen by a click), and the tone
  badge next to the title is outlined. Click the badge, or AUTO, to go back to AUTO / Hz. On the FTDX10's ACC
  scope a click still tunes the radio.
- Changing frequency (by 150 Hz or more), mode or VFO starts a new line and drops the audio of the old
  frequency; a tone chosen on the scope is released. Fine tuning of a few tens of Hz keeps the line.
- The settings, and which receivers use CW, are kept in this browser (`localStorage`).
- Like FT8, it works on the audio the page is playing: on a phone, when the page goes to the background, it pauses.

**Download from the Internet.** The decoder needs the ONNX runtime (12 MB, about 3 MB compressed on the wire)
and a model (narrow 1.2 MB, wide 15 MB): too large for the firmware. On first use the browser downloads them
from `cdn.jsdelivr.net`, at fixed versions (onnxruntime-web 1.24.3 and the models of
[e04/deepcw-engine](https://github.com/e04/deepcw-engine) at a fixed commit), checks each file against its
SHA-256 written in the page, and refuses a file that does not match (the panel shows `ERROR`, the reason is in
the tooltip). The files are kept in the browser's cache storage, so later visits work without downloading
them again. Only the browser goes to the Internet; the board opens nothing. For this the page's CSP allows
connections to `https://cdn.jsdelivr.net`, `blob:` scripts (the checked runtime is imported from memory) and
`'wasm-unsafe-eval'` (compiling the runtime's WebAssembly).

### Future improvement ideas

For technical evaluations, memory estimations on on-device FT8 decoding on the ESP32-S3, two-pass decoding, and other ideas, see [`docs/future_ideas.md`](docs/future_ideas.md).

## AI noise reduction

**AI NR**, next to MUTE above each receiver's scope, puts that receiver's audio through
[HamNoise](https://github.com/e04/HamNoise) by e04 (MIT License): a small neural network trained on amateur
radio audio that removes the noise and keeps the signal. It runs in this browser on the audio you hear only:
the scope, the FT8 and CW decoders, recordings, the radio and the board get the original audio.

- **Model** (DSP panel, NOISE SQUELCH GATE section): `AUTO` uses HamNoise's CW model in CW and its voice model
  in every other mode (SSB, AM, FM, DATA); `CW` or `VOICE` chooses one by hand. MAIN and SUB are separate on the FTX-1.
- **Strength**: how much of the cleaned audio is heard, the rest being the original (100 % = fully cleaned).
- The button is green while it works, amber while the model loads, red on an error (the original audio is
  played then; the reason is in the DSP panel).
- It adds about 0.1 s of delay to what you hear.
- **CPU**: the CW model is very light. The voice model is heavy: about half of one CPU core per receiver on a
  PC. The DSP panel shows the load; above 70 % it says the device is too slow (crackling audio): turn it off,
  or use it on one receiver only.
- The settings are kept in this browser.

**Download from the Internet.** Like the CW decoder, the models are too large for the firmware (CW 1.7 MB,
voice 4.3 MB of WebAssembly). On first use the browser downloads them from `cdn.jsdelivr.net` (the files of
[e04/HamNoise](https://github.com/e04/HamNoise) at a fixed commit), checks each against its SHA-256 written in
the page and refuses a file that does not match. They are kept in the browser's cache storage. Only the
browser goes to the Internet; the board opens nothing.

## OTA update

You can rewrite the firmware over the network.

```bash
source ~/esp/esp-idf/export.sh   # not needed inside the Dev Container
idf.py build
read -rs PW   # type the password (it is not kept in the command history)
curl -k -u "ota:$PW" -H "X-Requested-With: ftx1" -X POST --data-binary @build/network_remote.bin https://trx-remote.local/ota
unset PW
```

The `X-Requested-With: ftx1` header is required as CSRF protection (without it the request gets 403).

- Set the password with `4` in the serial menu (stored in NVS, at least 12 characters). Reading it with `read -rs` as above keeps it out of the command history. Keep `ota:` plus the password to 95 characters or fewer.
- Authentication is HTTP Basic and is sent over HTTPS only. The comparison is constant-time, and a failure is delayed by 1 second.
- The received image is validated by `esp_ota_end()` (magic, chip, checksum, SHA-256). A broken image is rejected with 400 and does not affect the running version.
- There are two 4 MB OTA slots (`ota_0` / `ota_1`, no factory app; see [`partitions.csv`](partitions.csv)). The update is written to the other slot, then the device switches to it and restarts.
- OTA cannot change the partition table. Moving a board from the old table (two 1700 KB slots) to this one needs one write over the cable: `tools/scrie_firmware.bat` option 3 writes the bootloader, the table, otadata and the app and leaves NVS (0x9000) alone, so the Wi-Fi and passwords stay.
- Rollback is enabled. A new version is marked "confirmed" once Wi-Fi is connected and the web server has started.
  A version that keeps failing to boot is restarted without being confirmed, and the bootloader goes back to the previous version.
- A second `POST /ota` during an update returns 409.
- **After changing the partition layout (for example moving from a single-app layout), the first update must be done over serial with `idf.py flash`.**
- Verified: authentication failure (401), broken image (400), a normal update (`ota_0` ↔ `ota_1`), and the state after restart.
  Not verified: actual rollback behavior, and updating while USB audio is being streamed.

## Self-signed certificate

On the first successful Wi-Fi boot, the board creates an EC P-256 key and a self-signed certificate.
It saves the pair in NVS and reuses it after power cycles, OTA and serial updates that preserve settings.
A newly generated certificate covers `trx-remote.local`, `trx-remote` and the station IPv4 address
assigned by DHCP at generation time. It is valid for ten years from the firmware build date in UTC.

Reserve the board’s IPv4 address in your router. A DHCP change or a firmware update leaves the
stored certificate in place, including older certificates without an IP entry. The startup log
warns when the current IPv4 address is missing. The authenticated `/status` endpoint reports
`cert_covers_ip` for the current station IPv4 address and `cert_sha256` for the certificate being served.

To replace the certificate and key, choose `c` (**Regenerate HTTPS certificate**) in the serial menu,
enter `yes`, then choose `7` to continue boot. This captures the IPv4 address assigned on that boot.
A VPN can route to the same LAN address. Access through a different address requires that address
to be covered by the certificate. The embedded fallback is used if the device has no usable
identity and cannot provision it. Its fingerprint and source appear in the startup log.

### Trusting the board certificate

Export only the public certificate from the browser’s certificate viewer (DER `.cer` or Base64/PEM).
Before importing, compare the full SHA-256 fingerprint in the viewer with the serial line
`Active HTTPS certificate (device NVS) SHA-256`. The new per-board certificate can be installed
as a local CA. Import only your own board’s verified certificate. A trusted CA can authorize other
TLS certificates if someone gets its private key, and raw flash access can expose that key unless
flash/NVS encryption is enabled.

- **Windows (Current User)**. Run `certmgr.msc`. Open **Trusted Root Certification Authorities**
  → **Certificates**, then **All Tasks** → **Import** and select the exported certificate.
  Reopen the browser and check both the IP address and `https://trx-remote.local`.
  [Microsoft certificate store documentation](https://learn.microsoft.com/en-us/windows-server/identity/ad-cs/inspect-certificates-manage-certificate-stores)
- **Android / Samsung**. Copy the exported public certificate to the phone. Open **Settings**
  → **Security and privacy** → **More security settings** → **Install from device storage**
  → **CA certificate**. Confirm and select the file. The labels vary by Android version.
  [Samsung menu example](https://eu.community.samsung.com/t5/seria-galaxy-s25/instalare-certificat/td-p/13421503)

After regenerating, remove the old trusted certificate, export the new one and import it again.

## Security notes

### Authentication

All endpoints (`/`, `/map`, `/status`, `/cat`, `POST /reboot`, `/stream.*`, `POST /ota`) are protected by HTTP Basic authentication. Use them over HTTPS only.

- Set the web ID and password with `9` (Web User ID, default `admin`) and `0` (Web Password) in the serial menu. They are stored in NVS.
- Passwords must be at least 12 characters. IDs cannot contain `:` or spaces.
- If no web password is set, the OTA ID / password is used instead. If neither is set, access is refused with 403 (it never opens without authentication).
- Five failures from the same client IP lock it out for 60 seconds and return 429.
- Requests that change state (`POST /cat`, `POST /reboot`, `POST /ota`) require the `X-Requested-With: ftx1` header and a same-origin `Origin` / `Sec-Fetch-Site` as CSRF protection.
- Responses carry `nosniff`, `X-Frame-Options: DENY`, `Referrer-Policy: no-referrer`, and a CSP.

### CAT command restrictions

Every command to the radio passes the allow-list in [`main/cat.c`](main/cat.c).

- The browser does not build CAT commands. `POST /cat` accepts only the JSON keys `target` / `freq` / `mode` / `vol` / `sq` / `notch` / `notch_hz` / `power` / `preamp` / `pa_band` / `att` / `rfg` / `agc` / `nb` / `nb_level` / `dnr` / `dnr_level` / `dnf` / `ifs` / `width` / `roofing` / `scope_span` / `scope_level` / `action`, or `raw` alone (the CAT console, below).
  Unknown keys, duplicate keys, and `cmd` (a raw command) are rejected with 400. `action` is only one of `swap` / `copy_main_to_sub` / `copy_sub_to_main` / `split_on` / `split_off`.
- Commands sent from the ESP32 to the radio use an allow list (`ID` `SV` `AB` `BA` `AI` `RI` `RM` `BY` `TX` `PS` `FA` `FB` `MD` `AG` `SQ` `BP` `PA` `RA` `RG` `GT` `NB` `NL` `NR` `RL` `BC` `IS` `SH` `RF` `SS` `PC` `EX` `ZI`), checked against the profile of the connected radio.
- **ZIN**, next to STEP on each receiver, sends CW AUTO ZERO IN (`ZI0;` MAIN, `ZI1;` SUB): the radio tunes that receiver onto the CW signal, as its own ZIN key does. It is enabled only while that receiver is in CW (CW-U / CW-L). The FTDX10 has one receiver, so only `ZI0;` is allowed there, whichever VFO is active. ZI is set only and cannot key the radio.
  For `SS` only the FTDX10's scope SPAN (`SS05` + 0–9 + `0000`) and LEVEL (`SS04`, read or −30.0 … +30.0 in 0.5 dB steps) items are allowed. For `EX` only the write that sets the DATA-mode audio output level to 100 is allowed (`EX010411` on the FTX-1, `EX010413` on the FTDX10).
  Supported radios also allow modulation menu queries and transmit VFO status (`FT;`). NET rigctl can set the active VFO, transmit VFO selection and filter width through radio-specific allow-listed commands. The page's **SPLIT** button selects the transmit VFO the same way (`FT1;`/`FT0;` on the FTX-1, `FT3;`/`FT2;` on the FTDX10); `FT;` is read every 500 ms so the button also follows the radio's own SPLIT key. These commands cannot key the radio. `TX;` reads TX status. Keying with `TX1;` requires a supported radio, the TX setting and a configured TX password. The allow-list always permits `TX0;` to release PTT.
  The ordinary CAT endpoint has no PTT operation.
- **CAT console** (off by default: enable it with **k** in the serial menu, then save with 6). The CAT LOG
  panel then has a field to type commands and **SEND** them (several per line, e.g. `FA;MD0;`;
  Up / Down recall earlier lines; the radio's answers appear in the log). Each command must pass the same allow-list
  as everything else (refused ones are logged as BLOCKED), **transmit (`TX1` / `TX2`) is always refused there** so
  keying stays with the TX panel (TX password, time limit, heartbeat), and if one command of a line is refused none
  is sent. At most 8 commands and 64 characters per line.
- Frequency settings are range-checked against the radio's profile (FTX-1 30 kHz – 470 MHz, FTDX10 30 kHz – 75 MHz); anything outside it is rejected.
- The allow list and the answer parsers are covered by a host test that runs without the board: `test/host_cat/run.sh`.
- Set `RPTT SELECT` / `PC KEYING` on the radio so that RTS / DTR cannot key it (see "Radio settings: no PTT on RTS / DTR" under Transmit). As a further safeguard, consider `TX INHIBIT` on the radio itself.
- Transmission that does not go through the ESP32 (microphone PTT, front-panel operation) cannot be prevented.

## Transmit

### Radio settings: no PTT on RTS / DTR (do this first)

Both radios have two USB serial ports: the **Enhanced COM Port** (CAT; this board uses it) and the
**Standard COM Port** (PTT, CW keying and digital modes through its **RTS / DTR** lines). This board never
uses RTS / DTR for PTT: it keys with the CAT command `TX1;` and keeps RTS / DTR low while it runs. While
the board restarts or boots (a few seconds, including a reset caused by opening or closing a serial terminal
on its USB-UART port), it is not running, and the radio's USB serial chip can pulse RTS / DTR. Yaesu notes
that a radio connected over USB can transmit briefly when the computer starts; the board is that computer
here. On an FTDX10 with RPTT SELECT on RTS / DTR, an owner saw a short transmission when a closed terminal
reset the board, and **also when the USB cable between the board and the radio was unplugged and when it
was plugged back in**. The pulse comes from the radio's own USB interface while it attaches or detaches,
before (or without) any software on the other end, so no firmware can prevent it: only the radio setting can.
An FTX-1 tested the same way, with RTS / DTR keying on in CW, DATA and RTTY, did **not** transmit, so this
is needed above all on the FTDX10; on the FTX-1 the settings below are a precaution.

Set the radio so that RTS / DTR cannot key it:

| Radio | Menu | Setting |
| --- | --- | --- |
| FTDX10 | **RPTT SELECT** (SSB, AM, FM, DATA, …) | **DAKY** (PTT from the rear DATA jack only; the menu has no OFF) |
| FTDX10 | **PC KEYING** (CW SETTING) | **OFF** |
| FTX-1 (precaution) | **RPTT SELECT** (SSB, AM, FM, DATA, RTTY, PRESET 1–5) | **OFF** (factory default) |
| FTX-1 (precaution) | **PC KEYING** | **OFF** (factory default) |

Remote PTT from this page keeps working: it does not use RTS / DTR.

**If you key the radio through RTS / DTR from a PC** (for example FSK RTTY, or CW through PC KEYING),
the setting is per mode: keep RTS / DTR only where you need it and use DAKY (FTDX10) or OFF (FTX-1) in the
other modes, e.g. RTS / DTR in RTTY only and DAKY in SSB, AM, FM and DATA (FT8). A USB pulse can then
key the radio only while it is in that mode, so do not plug, unplug or restart the board, or the PC, while
the radio is in RTTY (or in CW with PC KEYING on).

Remote PTT is off by default. At the serial menu, use `p` to set a TX password of at least 6
characters, `t` to enable TX, and `m` to set the 10–600 second time limit (default 120). Save with
option 6 to restart. Clearing the TX password also disables remote PTT. The page requires the web login
and the TX password. Five failed TX password attempts from the same IP trigger the 60-second lockout.
One browser can hold PTT at a time. Hold the button or Space while the page has focus. During confirmed TX, the PTT button and the
frequency readout for the radio-selected TX VFO turn red.
The TX panel offers PTT only, a 1,000 Hz test tone, and a 700 + 1,900 Hz two-tone test. Set MAIN to USB, LSB, DATA-USB or DATA-LSB.
Its **Power** slider shows the radio's TX power (CAT `PC`, read every 5 s) and, after **Authorize TX**, sets it:
5–100 W on the FTDX10; on the FTX-1 5–10 W on the field head or 5–100 W with the SPA-1, as the radio reports.
The power is written only over the authorized TX connection and only with remote TX enabled (never from
`/cat`, the CAT console or the network CAT servers), and not while transmitting.
On FTX-1, when SUB is selected for transmit, set SUB to one of those modes too. On FTDX10, select MAIN
for remote PTT. The firmware waits for fresh
CAT confirmation of the mode and the selected transmitter. Select USB modulation on the radio to use the test tones. The firmware leaves the modulation-source and transmit-gain menus under local control.

The device releases PTT on button/key release, a closed connection, 1.5 seconds without the owner's
heartbeat, the transmit time limit, loss of CAT or USB audio, high SWR, or missing or unexpected
transmit status. While keyed, the firmware waits for a CAT poll's replies before issuing the next poll, up to 1.5 seconds on the FTX-1 or 300 ms on
the FTDX10. Telemetry writes have a 1.2-second budget. A write timeout by itself
does not indicate CAT loss, which is reported after three seconds without a valid reply.
It sends TX off when a supported radio is identified, even with remote TX disabled. USB recovery, reboot, and OTA wait for
confirmed receive after a device-controlled transmission. During idle, automatic USB recovery can run
without CAT, but defers if a recent CAT report shows a transmission. If CAT is physically unavailable
while keyed, the firmware retries TX off when the link returns and blocks further PTT and power-cycles in
the meantime. Use the radio's local controls to stop transmission if the CAT connection stays down.

### Browser microphone

Authorize TX, select Microphone, and choose Enable microphone. The browser asks for permission on
that button. The level meter and gain control work in receive, without sending audio to the radio.
Use USB or LSB. Set SSB MOD SOURCE to USB or AUTO on the FTX-1. AUTO selects USB for CAT PTT.
On the FTDX10 use SSB MOD SOURCE REAR and REAR SELECT USB. The companion reads these menus and refuses microphone TX until they are
confirmed. Set the radio's modulation gain locally and use the microphone slider while watching ALC.

Hold PTT or Space on the focused page. Touch-and-hold PTT is supported on mobile. The browser sends
microphone audio only while it holds PTT and the firmware confirms it is the keyed owner. RX playback
is muted during remote TX and resumes after confirmed RX or after USB input restarts if it was paused. A release, focus loss, or ended microphone track releases PTT. Audio
packets never refresh the control heartbeat. The other automatic stops described above apply to voice.

`/status.tx_voice` reports the buffered milliseconds, the adaptive prefill target, underruns, discarded
frames and clock adjustment in parts per million. The adjustment uses an approximately one-second
exponential average of the buffer error. Empty playback produces silence. Enable microphone
only on the trusted HTTPS page. Stop the microphone with Disable microphone or by leaving the tab.

### USB test tone and SWR protection

The test tone defaults to −24 dBFS, with an adjustable range of −60 to −6 dBFS. The level of the two-tone
signal is bounded by the combined peak. Select DATA-USB with the radio's modulation source set to USB
for the phase 2 bench test. The firmware preserves the radio's modulation source and input gain settings.
Use a dummy load and minimum power.

The serial menu option `s` sets the SWR limit from 1.5:1 to 3.0:1 (default 3.0:1). While keyed, the
device reads RM5 (power) and RM6 (SWR). When PO reaches 3 on its raw 0–255 scale, the device checks the
absolute limit and stops on an increase of at least 0.5:1 over a three-sample reference taken after
one second of steady power. The rise also uses a three-sample average. Stale metering stops TX when
power is high or cannot be confirmed. The startup grace and maximum meter age are 2 seconds on the
FTX-1 and 1.1 seconds on the FTDX10. A radio Hi-SWR report requests an immediate stop after startup.
During the first 300 ms on FTDX10, an early Hi-SWR report is rechecked. A fresh normal report in that
window clears the transient flag. A persistent flag or missing confirmation stops TX at the end of
the window. The measured SWR limit and rise detector stay active throughout. After RX is confirmed,
the TX state remains stopping until the USB audio output finishes cleanup.

The panel and `/status` show both raw meters. SWR conversions use provisional, model-specific
[FTX-1](https://github.com/Hamlib/Hamlib/blob/master/rigs/yaesu/ftx1/ftx1.h)
and [FTDX10](https://github.com/Hamlib/Hamlib/blob/master/rigs/yaesu/ftdx10.h) Hamlib reference points.
Check these against the radio and known loads on the bench before relying on the numeric ratio.
In `/status`, the TX audio counters show the output sample rate, missed audio task deadlines and
bytes accepted by the USB driver. The `pcm_peak` value is the maximum absolute sample in
successfully completed USB packets since the audio output started. FTDX10 scope frames with an
out-of-range VFO during CAT TX are discarded and counted in `acc.vfo_range_tx`. In receive, the
same validation error is counted in `acc.rej.vfo_range`. The 16 KiB TX staging ring uses PSRAM, with its control block in internal RAM.
The UAC driver allocates USB transfer buffers when the output stream starts.

`/status.cat_write` reports write counts, timeouts, maximum elapsed microseconds and calls exceeding the
200 ms driver budget. It reports these since boot for all writes, writes while keyed, and RM5/RM6 batches.
Failed queued commands are visible in the page and CAT LOG. Compare `int_min` after identical runs with
ACC enabled, because it tracks the lowest free internal memory since boot.

### USB audio discovery

The firmware opens the radio's USB audio output interface to read its descriptors, with the output stream
stopped. The serial log lists the advertised formats. The authenticated `/status` endpoint reports
`tx_audio.present` and summarizes the first advertised format in `rates`, `channels`, and `bits`.
A continuous rate range uses `rate_min` and `rate_max` with an empty `rates` array.
During playback, `stream.active` indicates an open audio stream. The measured `stream.usb_in_hz` covers
the USB reads in that stream and remains available after playback ends. The serial log prints the final
rate and USB bus reset count when the stream ends.

Automatic USB recovery monitors the input stream. Discovery leaves the output stream stopped.

### FT8 transmit (phase 4)

Use the authenticated TX panel to enable FT8. Select USB with the station’s USB modulation source
or DATA-USB with USB modulation confirmed by CAT. On FTDX10 set **DATA MOD SOURCE = REAR** and
**REAR SELECT = USB** in MODE PSK/DATA. On FTX-1 set **MODE DATA → MOD SOURCE = USB**.
Set a TX time limit of at least 15 seconds.
Enter a message, audio frequency, level and the even/odd UTC slot. **Enable TX** queues up to
three frames. Adjust **Level** while FT8 is armed or transmitting and use the **ALC** meter
to monitor the radio. **Halt**, leaving the page or losing its heartbeat disarms TX. A decoded CQ can fill
a reply with your callsign and locator. It does not key the radio until you arm it.

The board prioritizes SNTP. The authenticated browser supplies a short-lived UTC estimate when
SNTP is unavailable. Keep the phone or PC clock synchronized, and check `ft8_tx` in `/status` for
the clock source, sample age and RTT. Browser RTT above 160 ms is discarded. When the current
estimate expires (15 s without a usable browser sample), or the selected clock offset changes by
more than 80 ms during a scheduled slot, FT8 disarms. Prefer SNTP on a high-latency VPN.
Keep the page foreground and focused while armed.

Use a dummy load at low power for initial tests. Decode the frame in WSJT-X using a second
receiver connected directly to the PC. Verify the decoded DT is within ±0.2 s and RX returns
after release. The serial log records the UTC slot, the RF audio target at slot +0.500 s,
the target for queuing PCM and when the first block was queued. This last timestamp is a
firmware reference for the measurement. Measure RF onset with a second receiver recording IQ
or audio against a synchronized UTC clock.

With the controlling PC clock synchronized, `schedule_minus_browser_ms` and
`browser_rtt_ms` help identify clock error before calibrating RF timing. Use low-RTT LAN
samples and confirm the result across several frames. Measure the demodulated FT8 signal,
and account for delay introduced by the receiver or recording path. Increase the radio's
playback lead by the repeatable RF lateness relative to `rf_target_utc`, in milliseconds.
`CONFIG_FT8_FTX1_AUDIO_LEAD_MS` and `CONFIG_FT8_FTDX10_AUDIO_LEAD_MS` are independent and
default to 20 ms. `CONFIG_FT8_NTP_SERVER` configures the SNTP server.


## Input validation and build settings

- The request body length, the JSON, and the USB audio format (channels, sample size, sample rate) are validated, and values outside the limits are rejected.
- FT8 decode results and similar text are HTML-escaped before display (XSS protection).
- Stack protection (`CONFIG_COMPILER_STACK_CHECK_MODE_STRONG`) and heap poisoning (`CONFIG_HEAP_POISONING_LIGHT`) are enabled.

### Remaining risks

- Secure Boot, flash encryption, and NVS encryption are not supported. Someone who gets the device physically may be able to read the passwords in NVS.
- The board stores its private HTTPS key in NVS. The firmware embeds a fallback certificate and key for boot failures.
- Consider replacing it with a certificate from a trusted CA before real use.
- **Do not expose it to the Internet.** Do not make this device reachable from outside, for example by port forwarding on your router. The firmware takes a reasonable level of security into account, but it is not designed on the assumption that it faces the Internet. Use it only on a trusted LAN.
- If you want to use it away from home, do not open a port; use a way into your LAN that is safe, such as a VPN.

## Limitations

- Only one client can receive the stream at a time.
- Only one audio input device is used (a second and later ones are ignored).
- Only UAC 1.0 devices are supported.
- Browser playback has a delay of a few hundred milliseconds.
- Phones: the page has a one-column phone layout. On Android / iOS, when the page goes to the background (screen off, another app in front) it closes its own stream and plays `/stream.wav` through an `<audio>` element, which phones keep playing like internet radio; it switches back when the page is visible again. The scope and FT8 pause while it is in the background. Lock-screen controls through Media Session. Whether a given phone keeps it running is up to its power settings; Wi-Fi power saving with the screen off can add gaps.

## Troubleshooting

### Flashing and the serial menu

- The serial port is not found, or `Failed to connect` is shown: check that the cable is not charge-only, and that the PC is connected to the **USB-UART port** (not the USB-OTG one).
- `Permission denied` on Linux: on Ubuntu and similar, run `sudo usermod -aG dialout $USER` and log in again (the group name differs between distributions).
- The log stops at `waiting for download`: the board entered flashing mode because **BOOT was held down** during a reset or power-up. Release BOOT and press the RST (reset) button, and it starts normally.
- The boot log stops at `Wi-Fi connection failed`: the board could not join Wi-Fi within 30 seconds. Press the RST button to restart, press `Enter` during the 3-second countdown to open the menu, and fix the SSID (`1`) and password (`2`). It cannot join a 5 GHz-only SSID.

### Browser

- `Access is disabled (no password configured)` (HTTP 403) is shown: the web password is not set. Set it with `0` in the serial menu.
- You cannot log in (401), or `Too many failed attempts` (429) is shown: check the ID and password. After 5 failures in a row from the same device (IP address), requests are refused for 60 seconds.

### Audio and connection

- `No USB audio device streaming` (HTTP 503): no USB audio input device is connected or recognized. Check `usb_audio` in the serial log.
- Audio drops out: open `/status` and check the following.
  - `heap_min` has fallen to a few KB: internal RAM is short. The setting that allocates TLS buffers only when needed (`CONFIG_MBEDTLS_DYNAMIC_BUFFER`) is enabled by default.
  - `stream.max_send_ms` is above about 330 ms (the 64 KB ring buffer), or `rssi` is worse than -75 dBm: check the Wi-Fi signal.
  - Running `audioDiag` in the browser console shows the receive wait (`maxReadMs`) and the browser processing time (`maxProcMs`).
- `trx-remote.local` does not open: check that it is on the same LAN / subnet, and try opening the IP address from the log directly.
- The browser refuses the connection: check that you opened `https://`, not `http://`.
- The device is not recognized: it may not be UAC 1.0, or a hub may not supply enough power.
- The scope (waterfall) does not match the frequency shown for VFO A / VFO B: this rarely happens. Please be kind to the rig and press the **A→B (A=B)** button to set the frequencies again.

## Source layout

| File | Role |
| --- | --- |
| [`main/main.c`](main/main.c) | Boot sequence (NVS → config check / serial menu → USB audio → Wi-Fi → web server → mDNS) |
| [`main/app_config.c`](main/app_config.c) | Wi-Fi / OTA / web credentials kept in NVS (non-volatile storage) |
| [`main/console_menu.c`](main/console_menu.c) | Interactive configuration menu on the USB UART serial console (SSID, passwords, OTA settings) |
| [`main/usb_audio.c`](main/usb_audio.c) | USB Host and [`espressif/usb_host_uac`](https://components.espressif.com/components/espressif/usb_host_uac): detects the input device, starts and reads the stream |
| [`main/wifi_sta.c`](main/wifi_sta.c) | Wi-Fi station connection (DHCP host name `trx-remote` by default) |
| [`main/web_server.c`](main/web_server.c) | HTTPS server (player page, status, PCM / WAV streams). The server and the stream task run on core 1, away from Wi-Fi and USB on core 0 |
| [`main/ota.c`](main/ota.c) | OTA update (`POST /ota`, Basic authentication, rollback) |
| [`main/cat.c`](main/cat.c) | CAT control (over the CP210x USB serial port: detects the radio from `ID;`, then reads and sets BUSY / S-meter / frequency / mode / volume / squelch with that radio's profile) |
| [`main/index.html`](main/index.html) | Browser player (Web Audio API) with a pseudo squelch |
| [`main/map.html`](main/map.html) | FT8 world map window (`/map`). The coastline data is embedded, so it works offline |

### Using it with another radio (a hint)

This fork supports the FTX-1 and the FTDX10. To add another radio, start from the comment at the top of [`main/cat.c`](main/cat.c): it lists every CAT command the firmware sends, with its parameters and the answers it reads, so you can compare it with that radio's CAT manual. Then add an entry to `s_profiles[]` (ID, frequency range, receivers, squelch range, the `EX` menu item, the polled commands) and, if the radio answers in another format, a parser. Whatever you change, extend `test/host_cat/test_cat.c` and re-check that no command that could make the radio transmit can pass the allow-list.

## Glossary

Short explanations of the technical terms used in this document.

### USB and the board

- **USB-OTG port**: the ESP32-S3 port that goes to the FTX-1. The ESP32-S3 acts as the USB host (the parent side).
- **USB-UART port**: the port for a PC (flashing, the serial menu) and for power.
- **UAC (USB Audio Class)**: the standard USB audio specification. The FTX-1 appears to a computer as a "USB microphone" in this way.
- **CDC**: a standard USB class for serial communication.
- **CP210x**: a USB-to-serial chip from Silicon Labs. The FTX-1's CAT port uses it. Some boards also use it for their USB-UART port, and a PC may then need its driver.
- **CAT (Computer Aided Transceiver)**: the way to control a radio from a computer. This project uses it to read and set the frequency, mode and so on.

### Network

- **mDNS**: lets you reach the device by a name such as `trx-remote.local` instead of an IP address.
- **DHCP**: the router's function that hands out IP addresses automatically.
- **LAN**: the network inside your home.
- **TLS / HTTPS**: encrypts the communication between the browser and the device.
- **SAN (Subject Alternative Name)**: the list of host names written in a certificate. If the name you open is not in it, the browser shows a warning.

### Security

- **CSRF**: an attack that makes your browser send a request you did not intend, from another web page. The `X-Requested-With` header blocks it.
- **XSS**: an attack that makes a web page run injected script. Displayed text is escaped to prevent it.
- **CSP (Content Security Policy)**: a rule the page gives the browser about what it may load and run.
- **Basic authentication**: a simple login method using an ID and a password. It is used together with HTTPS.

### Firmware and development

- **NVS (Non-Volatile Storage)**: an area in the microcontroller that keeps its contents when the power is off. Wi-Fi settings and passwords are stored here.
- **OTA (Over-The-Air)**: updating the firmware over the network, without a cable.
- **ESP-IDF**: the official development environment (SDK) for the ESP32.
- **Dev Container**: a development environment packaged in a Docker container.
- **FT8**: a digital mode for weak-signal contacts. The browser decodes it from the receive audio.
