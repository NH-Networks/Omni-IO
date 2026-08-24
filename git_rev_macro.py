import subprocess
import os

def get_version():
    if os.environ.get("RELEASE_VERSION"):
        return os.environ.get("RELEASE_VERSION")
    if os.environ.get("GITHUB_REF_TYPE") == "tag" and os.environ.get("GITHUB_REF_NAME"):
        return os.environ.get("GITHUB_REF_NAME")
    try:
        return subprocess.check_output(
            ["git", "describe", "--tags", "--always", "--dirty"],
            stderr=subprocess.DEVNULL
        ).decode().strip()
    except Exception:
        return "2.0.0"


def get_branch():
    try:
        branch = subprocess.check_output(
            ["git", "branch", "--show-current"],
            stderr=subprocess.DEVNULL
        ).decode().strip()
        if branch:
            return branch
    except Exception:
        pass
    return os.environ.get("GITHUB_REF_NAME") or os.environ.get("GITHUB_HEAD_REF") or "unknown"


version = get_version()
branch = get_branch()

try:
    Import("env")
except NameError:
    print(f"-DFIRMWARE_VERSION='\"{version}\"' -DFIRMWARE_BRANCH='\"{branch}\"'")
else:
    env.Append(CPPDEFINES=[
        ("FIRMWARE_VERSION", f'\\"{version}\\"'),
        ("FIRMWARE_BRANCH", f'\\"{branch}\\"'),
        ("FIRMWARE_ENVIRONMENT", f'\\"{env["PIOENV"]}\\"')
    ])