Hi all, here is a working proof of concept of **firmware updates over Zigbee** for ESPHome's native Zigbee support (ESP32-C6/H2, esp-zigbee-lib 2.0.5), as a regular OTA platform:

```yaml
ota:
  - platform: zigbee
    image_type: 0x0101   # one per kind of device
```

**How it works**
- It adds the ZCL OTA Upgrade client cluster (0x0019) to the first endpoint. The coordinator (Zigbee2MQTT, ZHA) is the OTA server.
- It writes the image through ESPHome's existing ESP-IDF OTA backend. App rollback, `safe_mode` and the `on_begin` / `on_progress` / `on_end` / `on_error` triggers all work as they do for the other OTA platforms.
- Every build also writes `firmware.zigbee.ota` next to `firmware.ota.bin`: a standard Zigbee OTA file with the image zlib-compressed, about 62 % of the size. The dashboard offers it as a download. `file_version` defaults to the build time, so each build is newer than the last.
- The `zigbee` component only gets a small listener hook in its existing action handler. Number and switch writes keep working.

**Code, write-up and test kit:** draft PR in my fork, mmornati/esphome#1 (https://github.com/mmornati/esphome/pull/1), branch [`zigbee-ota`](https://github.com/mmornati/esphome/tree/zigbee-ota). To try it:

```yaml
external_components:
  - source: github://mmornati/esphome@zigbee-ota
    components: [zigbee]
```

**Tested on hardware:** XIAO ESP32-C6 as router, Zigbee2MQTT 2.14.2 (EmberZNet coordinator). The device updated itself from Z2M:
- 361 KB in about 32 min. That's Z2M's default of 50-byte blocks, so it is slow but works.
- It survived the late duplicate blocks you get on a busy mesh.
- It rebooted into the new image, and the bootloader rollback was armed.

**Not done yet:** sleepy end devices (they need fast polling during the download), delta images, a ZHA quirk, nRF52/Zephyr, docs.

Credit to @jabla, whose [esphome-hiflow-zigbee](https://github.com/jabla/esphome-hiflow-zigbee) `zigbee_ota` component (MIT) showed the way for the file parsing and the block resync.

Would the maintainers (@luar123 @tomaszduda23) be open to a PR along these lines? I'm happy to adapt it to how you'd like Zigbee OTA to fit in.
