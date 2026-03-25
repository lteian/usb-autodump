# services/ftp_uploader_service.py
import asyncio
import ftplib
import os
from core.ftp_uploader import FTPUploader, UploadTask
from utils.config import get_ftp_config, load_config
from services.ws_manager import ws_manager
from services.state import state
import sys
from pathlib import Path

_parent = Path(__file__).resolve().parent.parent.parent
if str(_parent) not in sys.path:
    sys.path.insert(0, str(_parent))


def _human_size(size: int) -> str:
    for unit in ["B", "KB", "MB", "GB", "TB"]:
        if size < 1024:
            return f"{size:.1f}{unit}"
        size /= 1024
    return f"{size:.1f}PB"


def _task_to_queue_item(task: UploadTask) -> dict:
    return {
        "id": task.record_id,
        "filename": os.path.basename(task.local_path),
        "size": _human_size(task.file_size),
        "status": task.status,
        "progress": round(task.progress, 1) if hasattr(task, "progress") else 0,
    }


def init_ftp_uploader():
    uploader = FTPUploader()

    def on_upload(task: UploadTask):
        queue = uploader.get_queue()
        pending = sum(1 for t in queue if t.status == "pending")
        uploading = sum(1 for t in queue if t.status == "uploading")
        completed = sum(1 for t in queue if t.status in ("uploaded", "deleted"))
        asyncio.run(ws_manager.broadcast_upload_queue({
            "queue": [_task_to_queue_item(t) for t in queue],
            "total": len(queue),
            "pending": pending,
            "uploading": uploading,
            "completed": completed,
        }))

    uploader.add_callback(on_upload)
    uploader.start()
    state.ftp_uploader = uploader
    return uploader


def get_ftp_status() -> dict:
    uploader: FTPUploader = getattr(state, "ftp_uploader", None)
    if not uploader:
        return {"connected": False, "queue_size": 0}
    return {
        "connected": uploader.is_connected(),
        "queue_size": uploader.get_queue_size(),
    }


def get_upload_queue() -> dict:
    uploader: FTPUploader = getattr(state, "ftp_uploader", None)
    if not uploader:
        return {"queue": [], "total": 0, "pending": 0, "uploading": 0, "completed": 0}
    queue = uploader.get_queue()
    pending = sum(1 for t in queue if t.status == "pending")
    uploading = sum(1 for t in queue if t.status == "uploading")
    completed = sum(1 for t in queue if t.status in ("uploaded", "deleted"))
    return {
        "queue": [_task_to_queue_item(t) for t in queue],
        "total": len(queue),
        "pending": pending,
        "uploading": uploading,
        "completed": completed,
    }


def test_ftp_connection() -> dict:
    cfg = get_ftp_config()
    host = cfg.get("host", "")
    port = cfg.get("port", 21)
    username = cfg.get("username", "")
    password = cfg.get("password", "")
    use_tls = cfg.get("use_tls", False)

    if not host:
        return {"success": False, "message": "FTP 服务器未配置"}

    try:
        if use_tls:
            ftp = ftplib.FTP_TLS(timeout=10)
            ftp.connect(host, port)
            ftp.login(username, password)
            ftp.prot_p()
        else:
            ftp = ftplib.FTP(timeout=10)
            ftp.connect(host, port)
            ftp.login(username, password)
        ftp.quit()
        return {"success": True, "message": f"成功连接到 {host}:{port}"}
    except Exception as e:
        return {"success": False, "message": f"连接失败: {str(e)}"}
