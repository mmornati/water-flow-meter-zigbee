# [zigbee] OTA updates over Zigbee (`ota: - platform: zigbee`)

Firmware updates for ESPHome's native Zigbee devices (ESP32-C6/H2, esp-zigbee-lib 2.0.5), served by the Zigbee coordinator (Zigbee2MQTT, ZHA) like any other Zigbee OTA. Related discussion: esphome/esphome#3573 (feature request).

## Configuration

```yaml
ota:
  - platform: zigbee
    image_type: 0x0101        # required: one per kind of device
    # manufacturer_code: 0x131B  (default, Espressif)
    # file_version: 0x...        (default: build time, so every build is newer)
    # endpoint: 1                (default: first endpoint)
    # block_size: 223            (largest block the client asks for)
    # compress: true             (zlib image in the OTA file, ~62 % of the .bin)
    on_progress:
      - logger.log: {format: "OTA %.0f%%", args: [x]}
```

To try it without patching ESPHome (on a recent dev):

```yaml
external_components:
  - source: github://mmornati/esphome@zigbee-ota
    components: [zigbee]
```

## How it works

- **Cluster.** `zigbee` gets `add_ota_client_cluster()`, which puts the ZCL OTA Upgrade client (0x0019) on an endpoint. It also gets a `ZigbeeOTAListener` hook in its existing action handler. The handler is not replaced, so number/switch attribute writes keep working.
- **Writing the image.** `zigbee/ota` (`ZigbeeOTAComponent : ota::OTAComponent`) writes the image through ESPHome's ESP-IDF OTA backend (`ota::make_ota_backend()`). App rollback and `safe_mode` therefore behave as with `platform: esphome`, and the `on_begin` / `on_progress` / `on_end` / `on_error` / `on_abort` triggers work.
- **Parsing the file.** The client parses the OTA file as the blocks arrive. It reads the header length, then the sub-elements: tag `0x0000` is a raw image, tag `0xF100` is zlib, inflated with the ROM `tinfl` and a 32 KB window. Other tags are skipped.
- **Block resync.** The stack adds every block answer to `FileOffset`, a late duplicate's too, and then asks for the wrong next block. Out-of-place blocks are skipped, and `FileOffset` is set back by one block whenever it runs ahead of the data written. Overlapping blocks keep only their new part. The download gives up after 50 skipped blocks in a row.
- **Stall.** If no block arrives for 60 s, the download is abandoned and the client's upgrade status is reset.
- **Reboot.** After FINISH, the device reboots after the delay the server asks for, at least 2 s.
- **Build.** A CMake target runs `make_zigbee_ota.py` (stdlib only) after `gen_project_binary`. It writes `build/firmware.zigbee.ota`, which the dashboard offers as a third download ("Zigbee OTA format").

## Hardware test (2026-10-29)

Setup:
- Device: Seeed XIAO ESP32-C6 as router, with uptime and internal-temperature sensors.
- Coordinator: Zigbee2MQTT 2.14.2, EmberZNet 7.5.1, channel 25, with an external converter that sets `ota: true`.
- Image: about 580 KB, which gives a 361 KB OTA file.
- The update was sent with `bridge/request/device/ota_update/update`, in hex form.

| Run | Firmware | Result |
|---|---|---|
| 1 | first commit only | Aborted at 17 %. A late duplicate block moved `FileOffset` past the data, and the next block came after a gap. |
| 2 | first commit only | Aborted at 22 %, same cause. |
| 3 | with the resync fix | **Passed.** 361 082 bytes in 1941 s (about 186 B/s, Z2M default of 50-byte blocks with 250 ms between them). Two duplicate-block events, each recovered within 1 or 2 blocks. Image verified, rebooted into the new slot, `Bootloader rollback: supported`, `Boot seems successful`. Z2M installed = latest version, and the new behaviour was visible (30 s reports). |

In all three runs the device kept running and stayed joined. There were no crashes.

<details><summary>Key log lines, run 3</summary>

```
[zigbee.ota] Image offered: type 0x0101, version 0x6AE31525, 361082 bytes
[zigbee.ota] Starting update: version 0x6AE31525, 361082 bytes
[zigbee.ota] Block at 202000 (expected 202050), skipped
[zigbee.ota] FileOffset 202100 set to 202000 (data written up to 202050)
[zigbee.ota] Block at 202100 (expected 202050), skipped
[zigbee.ota] Block at 289300 (expected 289350), skipped
[zigbee.ota] FileOffset 289400 set to 289300 (data written up to 289350)
[zigbee.ota] Block at 289350 (expected 289400), skipped
[zigbee.ota] Progress: 100%
[zigbee.ota] Download complete: 361082 bytes, image 580272 bytes
[zigbee.ota] Image verified and selected for the next boot
[zigbee.ota] Rebooting into the new firmware in 2000 ms
--- reboot ---
[safe_mode]   Bootloader rollback: supported
[safe_mode] Boot seems successful
```
</details>

Also checked:
- `script/test_build_components.py -e config -c zigbee`: 4/4 pass, including the new `test-ota.esp32-c6-idf.yaml`.
- The full ESP32-C6 compile has no warnings.
- Unpacking the generated OTA file gives back exactly `firmware.ota.bin`.

## Known limits and next steps

- **Router only for now.** A sleepy end device needs fast polling during the download.
- **Speed.** It is set by the server: Z2M grants 50-byte blocks by default, which is about 30 min for a 360 KB file.
- **Abort after the image check.** If the server aborts after the image was checked, the device boots the old image again, but the OTA lock stays held until the next reboot.
- **Reports after the update.** For up to the max report interval, the device sends no reports by itself. Z2M re-configures it right after the announce and reads NaN from sensors that have not published yet. This comes from the base zigbee component.
- **Not done yet:** delta images, a ZHA quirk, Zephyr/nRF52, docs, and a periodic Query Next Image from the device.

## Credit

The file parsing, the block resync idea and the stall handling are adapted from [jabla/esphome-hiflow-zigbee](https://github.com/jabla/esphome-hiflow-zigbee) (`zigbee_ota`, MIT).
