# SPDX-FileCopyrightText: 2026 CloudAXS
# SPDX-License-Identifier: LicenseRef-CloudAXS-Proprietary
import os
import sys
import time
import urllib.request
import json

HOST = "http://10.10.33.15"
FS_PATH = os.path.join(".pio", "build", "LilyGoT3S3", "littlefs.bin")

if not os.path.exists(FS_PATH):
    print(f"Error: {FS_PATH} does not exist")
    sys.exit(1)

filesize = os.path.getsize(FS_PATH)
print(f"Filesystem binary found: {FS_PATH} ({filesize} bytes)")

print("Querying current device state...")
req = urllib.request.Request(f"{HOST}/api/info", headers={"User-Agent": "OTA-Updater"})
with urllib.request.urlopen(req, timeout=5) as resp:
    data = json.loads(resp.read().decode())
    print("Current device state:", data)

print("\n>>> Uploading filesystem to /api/filesystem ... <<<")
boundary = "----WebKitFormBoundaryOmniIoUpdate"
body_pre = (
    f"--{boundary}\r\n"
    f'Content-Disposition: form-data; name="file"; filename="littlefs.bin"\r\n'
    f"Content-Type: application/octet-stream\r\n\r\n"
).encode("utf-8")
body_post = f"\r\n--{boundary}--\r\n".encode("utf-8")

with open(FS_PATH, "rb") as f:
    file_bytes = f.read()

full_body = body_pre + file_bytes + body_post

upload_req = urllib.request.Request(
    f"{HOST}/api/filesystem",
    data=full_body,
    headers={
        "Content-Type": f"multipart/form-data; boundary={boundary}",
        "Content-Length": str(len(full_body)),
        "User-Agent": "OTA-Updater",
    },
    method="POST"
)

try:
    with urllib.request.urlopen(upload_req, timeout=60) as resp:
        print(f"Upload response ({resp.status}): {resp.read().decode('utf-8', errors='ignore')}")
except Exception as e:
    print(f"Upload finished or rebooting: {e}")

print("\nWaiting 10 seconds for device reboot...")
time.sleep(10)

start = time.time()
while time.time() - start < 60:
    try:
        req = urllib.request.Request(f"{HOST}/api/info", headers={"User-Agent": "OTA-Updater"})
        with urllib.request.urlopen(req, timeout=3) as resp:
            if resp.status == 200:
                final_data = json.loads(resp.read().decode())
                print("\nDevice is BACK ONLINE! New status:", json.dumps(final_data, indent=2))
                break
    except Exception:
        time.sleep(2)
else:
    print("Timeout waiting for device to return online.")
