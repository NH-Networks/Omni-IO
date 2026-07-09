import subprocess

def get_version():
    try:
        return subprocess.check_output(
            ["git", "describe", "--tags", "--always", "--dirty"],
            stderr=subprocess.DEVNULL
        ).decode().strip()
    except Exception:
        return "unknown"


def get_branch():
    try:
        return subprocess.check_output(
            ["git", "branch", "--show-current"],
            stderr=subprocess.DEVNULL
        ).decode().strip() or "unknown"
    except Exception:
        return "unknown"


version = get_version()
branch = get_branch()

try:
    Import("env")
except NameError:
    print(f"-DFIRMWARE_VERSION='\"{version}\"' -DFIRMWARE_BRANCH='\"{branch}\"'")
else:
    env.Append(CPPDEFINES=[
        ("FIRMWARE_VERSION", f'\\"{version}\\"'),
        ("FIRMWARE_BRANCH", f'\\"{branch}\\"')
    ])