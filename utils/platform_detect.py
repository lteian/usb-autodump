# utils/platform_detect.py
import platform

SYSTEM = platform.system()


def get_platform() -> str:
    if SYSTEM == "Windows":
        return "windows"
    elif SYSTEM == "Linux":
        distro = _get_linux_distro()
        return "linux"
    elif SYSTEM == "Darwin":
        return "macos"
    elif SYSTEM == "Android":
        return "android"
    return "unknown"


def _get_linux_distro() -> str:
    try:
        with open("/etc/os-release", "r", encoding="utf-8") as f:
            c = f.read().lower()
            if "uos" in c or "tongxin" in c or "tx-" in c:
                return "统信"
            if "kylin" in c:
                return "Kylin"
    except Exception:
        pass
    return "Unknown"


def is_windows() -> bool:
    return SYSTEM == "Windows"


def is_linux() -> bool:
    return SYSTEM == "Linux"


def is_android() -> bool:
    return SYSTEM == "Android"
