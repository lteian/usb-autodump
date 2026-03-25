# api/config.py
from fastapi import APIRouter, HTTPException
from typing import List
import sys
from pathlib import Path

_parent = Path(__file__).resolve().parent.parent.parent
if str(_parent) not in sys.path:
    sys.path.insert(0, str(_parent))

from utils.config import load_config, save_config, set_encryption_password, is_password_set, get_ftp_config
from models import ConfigSchema, ConfigUpdateSchema, PasswordSetSchema, FTPConfigSchema

router = APIRouter(prefix="/api/config", tags=["config"])


@router.get("", response_model=ConfigSchema)
def get_config():
    cfg = load_config()
    ftp_cfg = cfg.get("ftp", {})
    return ConfigSchema(
        local_path=cfg.get("local_path", ""),
        ftp=FTPConfigSchema(
            host=ftp_cfg.get("host", ""),
            port=ftp_cfg.get("port", 21),
            username=ftp_cfg.get("username", ""),
            password="",
            sub_path=ftp_cfg.get("sub_path", "/"),
            use_tls=ftp_cfg.get("use_tls", False),
            max_retry=ftp_cfg.get("max_retry", 3),
        ),
        video_extensions=cfg.get("video_extensions", []),
        auto_delete_local_after_upload=cfg.get("auto_delete_local_after_upload", True),
        auto_format_after_copy=cfg.get("auto_format_after_copy", False),
        max_concurrent_uploads=cfg.get("max_concurrent_uploads", 2),
        encryption_password_set=is_password_set(),
    )


@router.put("")
def update_config(data: ConfigUpdateSchema):
    cfg = load_config()
    if data.local_path is not None:
        cfg["local_path"] = data.local_path
    if data.ftp is not None:
        cfg["ftp"] = {
            "host": data.ftp.host,
            "port": data.ftp.port,
            "username": data.ftp.username,
            "password": data.ftp.password or cfg.get("ftp", {}).get("password", ""),
            "sub_path": data.ftp.sub_path,
            "use_tls": data.ftp.use_tls,
            "max_retry": data.ftp.max_retry,
        }
    if data.video_extensions is not None:
        cfg["video_extensions"] = data.video_extensions
    if data.auto_delete_local_after_upload is not None:
        cfg["auto_delete_local_after_upload"] = data.auto_delete_local_after_upload
    if data.auto_format_after_copy is not None:
        cfg["auto_format_after_copy"] = data.auto_format_after_copy
    if data.max_concurrent_uploads is not None:
        cfg["max_concurrent_uploads"] = data.max_concurrent_uploads
    save_config(cfg)
    return {"updated": True}


@router.post("/password")
def set_password(data: PasswordSetSchema):
    set_encryption_password(data.password)
    return {"password_set": True}
