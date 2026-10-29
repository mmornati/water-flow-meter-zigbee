**Title:** OTA Upgrade client: Upgrade End Request is sent with SUCCESS even when the application rejects the image at CHECK

**Environment**
- esp-zigbee-lib 2.0.5, ESP-IDF 5.5.6, ESP32-C6 (router)
- OTA server: Zigbee2MQTT 2.14.2 with an EmberZNet 7.5.1 coordinator

**What happens**
The application handles `EZB_ZCL_CORE_OTA_UPGRADE_CLIENT_PROGRESS_CB_ID`. When the download completes, it verifies the image (`esp_ota_end()`), and the verification fails because of a bad signature or a downgrade. The server still receives an Upgrade End Request with status SUCCESS. It answers with an Upgrade End Response and records the device as updated, although the device keeps running the old image.

**Tested**
- `out.result = EZB_ZCL_STATUS_INVALID_IMAGE` at `EZB_ZCL_OTA_UPGRADE_PROGRESS_CHECK`: the server reports success.
- `out.result = EZB_ZCL_STATUS_ABORT` at CHECK: the server reports success.
- Verifying at `EZB_ZCL_OTA_UPGRADE_PROGRESS_APPLY` and returning ABORT there, as in `examples/ota_upgrade/ota_client`: the server reports success. APPLY comes about 200 ms after CHECK, after the Upgrade End exchange.

**Expected**
The result returned at CHECK becomes the status of the Upgrade End Request, as ZCL 11.13.9 describes: INVALID_IMAGE (0x96) or ABORT (0x95). The server then knows the upgrade failed.

**Ask**
- Could `out.result` at CHECK be used for the Upgrade End status?
- Alternatively, could the library expose an API to send Upgrade End Request with a given status?
