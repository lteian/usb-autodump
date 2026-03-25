# services/stats_service.py
import asyncio
import os
import sys
from pathlib import Path
from services.ws_manager import ws_manager
from services.state import state
from services import usb_monitor_service, ftp_uploader_service, copy_engine_service

_parent = Path(__file__).resolve().parent.parent.parent
if str(_parent) not in sys.path:
    sys.path.insert(0, str(_parent))

from core.file_record import get_all_records
from utils.config import get_local_path


def _human_size(size: int) -> str:
    for unit in ["B", "KB", "MB", "GB", "TB"]:
        if size < 1024:
            return f"{size:.1f}{unit}"
        size /= 1024
    return f"{size:.1f}PB"


async def broadcast_stats():
    devices = usb_monitor_service.get_current_devices()
    all_records = []
    try:
        all_records = get_all_records()
    except Exception:
        pass

    device_stats = []
    for dev in devices:
        drive = dev["drive_letter"]
        drive_records = [r for r in all_records if r.get("usb_drive") == drive]
        copied = sum(1 for r in drive_records if r.get("status") in ("copied", "uploaded", "deleted"))
        device_stats.append({
            "drive": drive,
            "label": dev.get("label", "U盘"),
            "total_files": len(drive_records),
            "copied": copied,
            "pending": len(drive_records) - copied,
        })

    # 计算存储使用
    local_path = get_local_path()
    total_size = 0
    used_size = 0
    try:
        if os.path.exists(local_path):
            for entry in os.scandir(local_path):
                if entry.is_dir():
                    for root, _, files in os.walk(entry.path):
                        for f in files:
                            fp = os.path.join(root, f)
                            try:
                                s = os.path.getsize(fp)
                                total_size += s
                                used_size += s
                            except OSError:
                                pass
    except Exception:
        pass

    ftp_status = ftp_uploader_service.get_ftp_status()

    total_label = _human_size(total_size) if total_size else "0B"
    used_label = _human_size(used_size) if used_size else "0B"
    percent = round(used_size / total_size * 100, 1) if total_size > 0 else 0

    await ws_manager.broadcast_stats({
        "devices": device_stats,
        "storage": {
            "used": used_label,
            "total": total_label,
            "percent": percent,
        },
        "ftp_connected": ftp_status.get("connected", False),
    })


_stats_task: asyncio.Task | None = None


def start_stats_broadcast(interval: float = 5.0):
    async def loop():
        while True:
            try:
                await broadcast_stats()
            except Exception:
                pass
            await asyncio.sleep(interval)

    global _stats_task
    _stats_task = asyncio.create_task(loop())
