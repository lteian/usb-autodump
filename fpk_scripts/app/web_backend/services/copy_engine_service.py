# services/copy_engine_service.py
import asyncio
import os
from pathlib import Path
from core.copy_engine import CopyEngine, CopyTask
from core.disk_tool import eject_drive
from services.ws_manager import ws_manager
from services.state import state
import sys

_parent = Path(__file__).resolve().parent.parent.parent
if str(_parent) not in sys.path:
    sys.path.insert(0, str(_parent))


def _task_to_dict(task: CopyTask) -> dict:
    return {
        "usb_drive": task.usb_drive,
        "src_path": task.src_path,
        "dst_path": task.dst_path,
        "file_size": task.file_size,
        "record_id": task.record_id,
        "progress": task.progress,
        "status": task.status,
        "error_msg": task.error_msg,
    }


def init_copy_engine():
    engine = CopyEngine()

    def on_progress(drive: str, current_index: int, total: int, task: CopyTask):
        current_file = os.path.basename(task.src_path)
        file_progress = task.progress
        overall_progress = ((current_index - 1) / total * 100) + (file_progress / total) if total > 0 else 0
        asyncio.run(ws_manager.broadcast_copy_progress({
            "drive": drive,
            "current_file": current_file,
            "current_index": current_index,
            "total_files": total,
            "file_progress": round(file_progress, 1),
            "overall_progress": round(overall_progress, 1),
        }))

    def on_done(drive: str, tasks: list):
        asyncio.run(ws_manager.broadcast_copy_progress({
            "drive": drive,
            "current_file": "",
            "current_index": len(tasks),
            "total_files": len(tasks),
            "file_progress": 100.0,
            "overall_progress": 100.0,
            "done": True,
        }))

    engine.add_progress_callback(on_progress)
    engine.add_done_callback(on_done)
    state.copy_engine = engine
    return engine


def start_copy(drive_letter: str) -> list:
    engine: CopyEngine = getattr(state, "copy_engine", None)
    if not engine:
        init_copy_engine()
        engine = state.copy_engine
    tasks = engine.start_copy(drive_letter)
    return [_task_to_dict(t) for t in tasks]


def cancel_copy(drive_letter: str):
    engine: CopyEngine = getattr(state, "copy_engine", None)
    if engine:
        engine.cancel_copy(drive_letter)


def get_tasks(drive_letter: str) -> list:
    engine: CopyEngine = getattr(state, "copy_engine", None)
    if not engine:
        return []
    return [_task_to_dict(t) for t in engine.get_tasks(drive_letter)]


def is_copying(drive_letter: str) -> bool:
    engine: CopyEngine = getattr(state, "copy_engine", None)
    if not engine:
        return False
    return engine.is_copying(drive_letter)
