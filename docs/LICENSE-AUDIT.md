# Omni-IO License Audit

**Date:** 2026-08-31
**Author:** CloudAXS (Automated Audit)

This document classifies all files in this repository based on their origin, identifying explicitly proprietary CloudAXS contributions versus upstream and third-party code. This audit governs the application of `LICENSE-CLOUDAXS` and `NOTICE`.

## CloudAXS Proprietary (CloudAXS Nieuw)
These files were fully created by CloudAXS. They do not contain upstream code and are exclusively licensed under `LICENSE-CLOUDAXS`.
* `src/esphome_server.cpp`
* `include/esphome_server.h`
* `src/SX126xHelpers.cpp`
* `include/SX126xHelpers.h`
* `include/sx126xRegs.h`
* `docs/API_AND_MQTT_REFERENCE.md`
* `docs/HARDWARE_SETUP.md`
* `docs/HOME_ASSISTANT.md`
* `docs/PAIRING_GUIDE.md`
* `docs/TROUBLESHOOTING.md`
* `knowledge.md`
* `extras/web_interface_data/img/logo.svg`
* `scratch/upload_ota.py`
* `tools/test_esphome_api.py`
* `partitions_4MB_OTA.csv`
* `.github/ISSUE_TEMPLATE/bug_report.md`
* `.github/ISSUE_TEMPLATE/feature_request.md`
* `.github/pull_request_template.md`

## Upstream / CloudAXS Modified (Apache License 2.0)
These files originated from the upstream repository (`djbenbe/miopen.io`) and have been modified by CloudAXS. They remain licensed under the Apache License 2.0.
* `extras/web_interface_data/index.html`
* `extras/web_interface_data/css/style.css`
* `extras/web_interface_data/js/*.js`
* `extras/web_interface_data/lang/*.json`
* `src/web_server_handler.cpp`
* `src/main.cpp`
* `src/mqtt_handler.cpp`
* `src/oled_display.cpp`
* `src/syslog_helper.cpp`
* `src/interact.cpp`
* `src/wifi_helper.cpp`
* `src/SX1276Helpers.cpp`
* `src/iohcOtherDevice2W.cpp`
* `src/iohcRemote1W.cpp`
* `include/iohcRemote1W.h`
* `include/mqtt_handler.h`
* `include/nvs_helpers.h`
* `include/oled_display.h`
* `include/user_config.h`
* `include/web_server_handler.h`
* `.github/workflows/branch_build.yml`
* `.github/workflows/build_and_release.yml`
* `git_rev_macro.py`
* `platformio.ini`
* `COMMANDS.md`
* `README.md`
* `.gitignore`

## Upstream Unmodified (Apache License 2.0)
All other `.cpp` and `.h` files not listed above remain unmodified upstream code and are governed by the Apache License 2.0.

## Third-Party / Status Unknown
* `extras/web_interface_data/img/*.svg` (excluding `logo.svg` and `radio.svg`). See `ASSETS-LICENSES.md` for details.
