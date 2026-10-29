# Zigbee OTA for ESPHome: POC

`ota: - platform: zigbee` lets Zigbee2MQTT (or ZHA) update an ESPHome Zigbee
device over the Zigbee network. It is built on ESPHome dev at `5071968`
(the commit your clone is on).

## What's here

| File | What it is |
|---|---|
| `0001-zigbee-Add-OTA-updates-over-Zigbee-...patch` | The ESPHome change (8 files, ~820 lines) |
| `0002-zigbee-OTA-recover-from-duplicate-...patch` | Fix found in the hardware test: recover from duplicate/out-of-order blocks |
| `zigbee-ota-test.yaml` | Test config for a spare XIAO ESP32-C6 (router, uptime + chip temperature) |
| `esphome_ota_test.mjs` | Zigbee2MQTT external converter for the test device, with `ota: true` |

## What the change does

- **`zigbee` component:** a small listener hook in the existing ZCL action
  handler. The OTA callbacks reach the OTA platform and the handler is not
  replaced, so `number`/`switch` writes keep working. It also adds
  `add_ota_client_cluster()`, which puts the OTA Upgrade client cluster
  (0x0019) on an endpoint (the first one by default).
- **`zigbee/ota` platform:**
  - Uses ESPHome's own ESP-IDF OTA backend, so app rollback and `safe_mode`
    work as they do for `platform: esphome`.
  - Unpacks a raw or zlib-compressed image from the OTA file as the blocks arrive.
  - The usual `on_begin` / `on_progress` / `on_end` / `on_error` triggers work.
  - If no block arrives for 60 s, it gives up the download.
  - Reboots after the delay the server asks for.
- **Build:** every compile also writes
  `.esphome/build/<name>/build/firmware.zigbee.ota`, which is about 62 % of
  the `.bin`. The dashboard lists it as a third download, "Zigbee OTA format".
  `file_version` defaults to the build time, so each build is newer than the
  one before.

Options: `image_type` (required, give each kind of device its own),
`manufacturer_code` (default 0x131B, Espressif), `file_version`, `endpoint`,
`block_size` (default 223), `compress` (default true).

## Checked in the cloud container

- The full ESP32-C6 compile passes with no warnings. RAM is 18.8 %, flash 32.8 %.
- `firmware.zigbee.ota` was generated: 361 KB from a 580 KB image. Its header
  parses, and unpacking it gives back exactly `firmware.ota.bin`.
- `script/test_build_components.py -e config -c zigbee`: 4/4 pass, including
  the new `test-ota.esp32-c6-idf.yaml`.
- **Hardware test, 2026-10-29, PASS** (XIAO ESP32-C6 router, Z2M 2.14.2, EmberZNet 7.5.1, channel 25):
  - The C6 updated itself over Zigbee from v1 0x6AE314BD to v2 0x6AE31525: 361 KB
    in 32 min (about 186 B/s, Z2M's default of 50-byte blocks with 250 ms between them).
  - Two late duplicate blocks happened during the download and were recovered (patch 0002).
  - It rebooted into v2, logged "Bootloader rollback: supported" and
    "Boot seems successful", and Z2M showed installed = latest.
  - The first two tries, without patch 0002, aborted at 17 % and 22 % on the same
    duplicate-block event.

## Test on the spare XIAO ESP32-C6

1. **Apply the patch in your ESPHome clone**, then install it into a venv:
   ```bash
   cd ~/Projects/esphome
   git switch -c zigbee-ota
   git am /path/to/0001-zigbee-Add-OTA-updates-over-Zigbee-*.patch
   python3 -m venv venv && source venv/bin/activate && pip install -e .
   ```
2. **Install the Z2M converter.** Copy `esphome_ota_test.mjs` into
   `<z2m data>/external_converters/` and restart Z2M. Z2M 2.x loads that
   folder by itself.
3. **First flash, by USB.** This is the only USB flash; it also installs the
   bootloader with rollback enabled.
   ```bash
   esphome run zigbee-ota-test.yaml   # pick the USB port
   ```
   Permit join in Z2M. The device should show up as `ESPHomeOTATest`, with
   uptime, chip temperature and a firmware update entity.
4. **Build a second version.** Change something you can see, for example the
   uptime `update_interval` from 60s to 30s. Then compile:
   ```bash
   esphome compile zigbee-ota-test.yaml
   ```
   The log ends with
   `Zigbee OTA file firmware.zigbee.ota: version 0x..., ... bytes`.
5. **Give the file to Z2M.** Copy
   `.esphome/build/zigbee-ota-test/build/firmware.zigbee.ota` into the Z2M
   data folder, then publish:
   ```
   topic:   zigbee2mqtt/bridge/request/device/ota_update/update
   payload: {"id": "<friendly name>", "url": "<path as Z2M sees it>/firmware.zigbee.ota"}
   ```
   If you run the HA add-on, the path usually starts with `/config/zigbee2mqtt/`.
6. **Watch the serial log** (`esphome logs zigbee-ota-test.yaml`):
   - `zigbee.ota` should log `Starting update`, then `Progress: 10%` …
     `Download complete`, then `Image verified and selected for the next boot`.
   - Then `Rebooting into the new firmware`.
   - After the reboot, Z2M should show the new file version and the device
     should behave as version 2.

   Expect a few minutes for the 361 KB file. The speed depends on the block
   size Z2M grants.
7. **Optional rollback check.** Make a version 3 that crashes at boot, for
   example a lambda in `on_boot` that dereferences `nullptr`, and push it
   over Zigbee. The bootloader should fall back to version 2.

## Things to watch for / known limits

- **Who starts the update.** Z2M sends Image Notify and the esp-zigbee stack
  answers with Query Next Image on its own. jabla reports this flow working
  with Z2M 2.14.1 for the same stack, and this is the main thing your test
  confirms for the ESPHome version. If Z2M times out on "check", tell me and
  I'll add a periodic or on-demand query from the device.
- **Router only for now.** A sleepy end device would need fast polling during
  the download (a follow-up).
- If the server aborts *after* the image was verified, the device boots the
  old image again, but it refuses other OTAs until the next reboot.
- Flash erase runs on the Zigbee task in 64 KB steps. Each step can delay one
  block, and the 10 s × 3 block retry covers that.
- **Follow-ups, not in the POC:** delta images, ZHA quirk, Zephyr/nRF52, docs PR.

Credit: file parsing, block resync and stall handling are adapted from
[jabla/esphome-hiflow-zigbee](https://github.com/jabla/esphome-hiflow-zigbee) (MIT).
