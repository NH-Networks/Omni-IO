import subprocess

def get_version():
    try:
        return subprocess.check_output(
            ["git", "describe", "--tags", "--always", "--dirty"],
            stderr=subprocess.DEVNULL
        ).decode().strip()
    except Exception:
        return "unknown"


version = get_version()

try:
    Import("env")
except NameError:
    print(f"-DFIRMWARE_VERSION='\"{version}\"'")
else:
    env.Append(CPPDEFINES=[("FIRMWARE_VERSION", f'\\"{version}\\"')])
