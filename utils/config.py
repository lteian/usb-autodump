import json
from pathlib import Path
from utils.crypto import encrypt, decrypt

CONFIG_PATH = Path.home() / ".config" / "usb_autodump" / "config.json"

_default_config = {
    "local_path": "D:/U盘转储",
    "ftp": {
        "host": "",
        "port": 21,
        "username": "",
        "password": "",
        "sub_path": "/",
        "use_tls": False,
        "max_retry": 3
    },
    "video_extensions": [".mp4", ".avi", ".mkv", ".mov", ".wmv", ".flv", ".webm", ".m4v", ".mpg", ".mpeg"],
    "auto_delete_local_after_upload": True,
    "auto_format_after_copy": False,
    "max_concurrent_uploads": 2,
    "usb_paths": {},
    "encryption_password": ""
}


def load_config() -> dict:
    if not CONFIG_PATH.exists():
        CONFIG_PATH.parent.mkdir(parents=True, exist_ok=True)
        save_config(_default_config)
        return _default_config.copy()
    try:
        with open(CONFIG_PATH, "r", encoding="utf-8") as f:
            cfg = json.load(f)
        # 密码已加密存着，需要先拿到 encryption_password 才能解密
        enc_pwd = cfg.get("encryption_password", "")
        ftp_cfg = cfg.get("ftp", {})
        if enc_pwd and ftp_cfg.get("password"):
            ftp_cfg["password"] = decrypt(ftp_cfg["password"], enc_pwd)
        return cfg
    except Exception:
        return _default_config.copy()


def save_config(config: dict):
    CONFIG_PATH.parent.mkdir(parents=True, exist_ok=True)
    to_save = dict(config)
    enc_pwd = to_save.get("encryption_password", "")
    ftp_cfg = dict(to_save.get("ftp", {}))
    if enc_pwd and ftp_cfg.get("password"):
        ftp_cfg["password"] = encrypt(ftp_cfg["password"], enc_pwd)
    to_save["ftp"] = ftp_cfg
    with open(CONFIG_PATH, "w", encoding="utf-8") as f:
        json.dump(to_save, f, ensure_ascii=False, indent=2)


def get_encryption_password() -> str:
    return load_config().get("encryption_password", "")


def set_encryption_password(password: str):
    cfg = load_config()
    cfg["encryption_password"] = password
    save_config(cfg)


def is_password_set() -> bool:
    return bool(get_encryption_password())


def get_video_extensions() -> list:
    return load_config().get("video_extensions", [".mp4"])


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
