import json
import os
from pathlib import Path
from utils.crypto import encrypt, decrypt

CONFIG_PATH = Path(__file__).parent.parent / "config.json"

_default_config = {
    "local_path": "D:/U盘转储",
    "ftp": {
        "host": "",
        "port": 21,
        "username": "",
        "password": "",          # 加密存储
        "sub_path": "/",
        "use_tls": False,
        "max_retry": 3
    },
    "video_extensions": [".mp4", ".avi", ".mkv", ".mov", ".wmv", ".flv", ".webm", ".m4v", ".mpg", ".mpeg"],
    "auto_delete_local_after_upload": True,
    "auto_format_after_copy": False,
    "max_concurrent_uploads": 2,
    "usb_paths": {}             # {"E:": "/path/to/E", "F:": "/path/to/F"}
}


def load_config() -> dict:
    if not CONFIG_PATH.exists():
        save_config(_default_config)
        return _default_config.copy()
    try:
        with open(CONFIG_PATH, "r", encoding="utf-8") as f:
            cfg = json.load(f)
        # 解密密码
        if "ftp" in cfg and cfg["ftp"].get("password"):
            cfg["ftp"]["password"] = decrypt(cfg["ftp"]["password"])
        return cfg
    except Exception:
        return _default_config.copy()


def save_config(config: dict):
    CONFIG_PATH.parent.mkdir(parents=True, exist_ok=True)
    # 加密密码再存
    to_save = dict(config)
    if "ftp" in to_save and to_save["ftp"].get("password"):
        to_save["ftp"] = dict(to_save["ftp"])
        to_save["ftp"]["password"] = encrypt(to_save["ftp"]["password"])
    with open(CONFIG_PATH, "w", encoding="utf-8") as f:
        json.dump(to_save, f, ensure_ascii=False, indent=2)


def get_video_extensions() -> list:
    cfg = load_config()
    return cfg.get("video_extensions", [".mp4"])


def get_local_path(usb_drive: str = None) -> str:
    cfg = load_config()
    if usb_drive and usb_drive in cfg.get("usb_paths", {}):
        return cfg["usb_paths"][usb_drive]
    return cfg.get("local_path", "D:/U盘转储")


def get_ftp_config() -> dict:
    return load_config().get("ftp", {})


def set_usb_path(usb_drive: str, path: str):
    cfg = load_config()
    cfg.setdefault("usb_paths", {})[usb_drive] = path
    save_config(cfg)
