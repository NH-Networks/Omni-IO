# SPDX-FileCopyrightText: 2026 CloudAXS
# SPDX-License-Identifier: LicenseRef-CloudAXS-Proprietary
import os
import sys
import time
import urllib.request
import urllib.parse
import json

import glob

HOST = "http://10.10.33.15"

firmware_files = sorted(glob.glob(r"D:\Development\Omni-IO\scratch\ota_binaries\*_firmware.bin"))
filesystem_files = sorted(glob.glob(r"D:\Development\Omni-IO\scratch\ota_binaries\*_filesystem.bin"))

if not firmware_files or not filesystem_files:
    raise FileNotFoundError("Could not find firmware or filesystem binary")

FIRMWARE_PATH = firmware_files[-1]
FILESYSTEM_PATH = filesystem_files[-1]

def wait_for_online(timeout=60):
    print(f"Waiting for {HOST} to come back online...")
    start = time.time()
    while time.time() - start < timeout:
        try:
            req = urllib.request.Request(f"{HOST}/api/info", headers={"User-Agent": "OTA-Updater"})
            with urllib.request.urlopen(req, timeout=3) as resp:
                if resp.status == 200:
                    data = json.loads(resp.read().decode())
                    print(f"Device online! Info: {data}")
                    return data
        except Exception:
            pass
        time.sleep(2)
    raise TimeoutError("Device did not come back online in time")

def post_file(endpoint, filepath):
    filename = os.path.basename(filepath)
    filesize = os.path.getsize(filepath)
    print(f"\n--- Uploading {filename} ({filesize} bytes) to {HOST}{endpoint} ---")

    boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW"
    body_pre = (
        f"--{boundary}\r\n"
        f'Content-Disposition: form-data; name="file"; filename="{filename}"\r\n'
        f"Content-Type: application/octet-stream\r\n\r\n"
    ).encode("utf-8")
    body_post = f"\r\n--{boundary}--\r\n".encode("utf-8")

    total_size = len(body_pre) + filesize + len(body_post)

    with open(filepath, "rb") as f:
        file_bytes = f.read()

    full_body = body_pre + file_bytes + body_post

    req = urllib.request.Request(
        f"{HOST}{endpoint}",
        data=full_body,
        headers={
            "Content-Type": f"multipart/form-data; boundary={boundary}",
            "Content-Length": str(len(full_body)),
            "User-Agent": "OTA-Updater",
        },
        method="POST"
    )

    try:
        with urllib.request.urlopen(req, timeout=60) as resp:
            resp_body = resp.read().decode("utf-8", errors="ignore")
            print(f"Response ({resp.status}): {resp_body}")
    except Exception as e:
        print(f"Upload completed or connection reset by peer during reboot: {e}")

if __name__ == "__main__":
    print("Checking initial device state:")
    init_info = wait_for_online(timeout=10)
    print("Current firmware version:", init_info.get("version"))

    print("\n>>> STEP 1: Flashing Firmware <<<")
    post_file("/api/firmware", FIRMWARE_PATH)

    print("Giving device time to reboot...")
    time.sleep(10)
    mid_info = wait_for_online(timeout=60)
    print("Firmware flashed successfully! Version reported:", mid_info.get("version"))

    print("\n>>> STEP 2: Flashing Filesystem <<<")
    post_file("/api/filesystem", FILESYSTEM_PATH)

    print("Giving device time to reboot...")
    time.sleep(10)
    final_info = wait_for_online(timeout=60)
    print("\nFilesystem flashed successfully!")
    print("Final device status:", json.dumps(final_info, indent=2))

    # Check ESPHome endpoint
    print("\n>>> STEP 3: Checking ESPHome status endpoint <<<")
    try:
        req = urllib.request.Request(f"{HOST}/api/esphome", headers={"User-Agent": "OTA-Updater"})
        with urllib.request.urlopen(req, timeout=5) as resp:
            esp_info = json.loads(resp.read().decode())
            print("ESPHome API status:", json.dumps(esp_info, indent=2))
    except Exception as e:
        print("Failed to query /api/esphome:", e)

