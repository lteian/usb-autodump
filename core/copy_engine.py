# core/copy_engine.py
import os
import threading
from pathlib import Path
from typing import Callable, List, Optional
from utils.config import get_video_extensions, get_local_path
from utils.logger import get_logger
from core.file_record import add_record, update_status

logger = get_logger()


class CopyTask:
    def __init__(self, usb_drive: str, src_path: str, dst_path: str, file_size: int, record_id: int):
        self.usb_drive = usb_drive
        self.src_path = src_path
        self.dst_path = dst_path
        self.file_size = file_size
        self.record_id = record_id
        self.progress: float = 0.0
        self.status: str = "pending"
        self.error_msg: Optional[str] = None


class CopyEngine:
    def __init__(self):
        self._tasks: dict[str, List[CopyTask]] = {}
        self._threads: dict[str, threading.Thread] = {}
        self._progress_callbacks: List[Callable] = []
        self._done_callbacks: List[Callable] = []
        self._cancel_flags: dict[str, bool] = {}
        self._lock = threading.Lock()

    def add_progress_callback(self, cb: Callable):
        self._progress_callbacks.append(cb)

    def add_done_callback(self, cb: Callable):
        self._done_callbacks.append(cb)

    def start_copy(self, drive_letter: str) -> List[CopyTask]:
        video_exts = get_video_extensions()
        local_base = Path(get_local_path(drive_letter))
        dest_dir = local_base / drive_letter.rstrip("\\/")
        dest_dir.mkdir(parents=True, exist_ok=True)

        tasks = []
        with self._lock:
            self._cancel_flags[drive_letter] = False

        for root, dirs, files in os.walk(drive_letter):
            if self._cancel_flags.get(drive_letter, False):
                break
            for filename in files:
                ext = Path(filename).suffix.lower()
                if ext not in video_exts:
                    continue
                src_path = os.path.join(root, filename)
                rel_path = os.path.relpath(src_path, drive_letter)
                dst_path = str(dest_dir / rel_path)
                try:
                    file_size = os.path.getsize(src_path)
                except OSError:
                    continue
                record_id = add_record(drive_letter, src_path, dst_path, file_size)
                task = CopyTask(drive_letter, src_path, dst_path, file_size, record_id)
                tasks.append(task)

        with self._lock:
            self._tasks[drive_letter] = tasks

        def worker():
            for i, task in enumerate(tasks):
                if self._cancel_flags.get(drive_letter, False):
                    break
                try:
                    update_status(task.record_id, "copying")
                    task.status = "copying"
                    os.makedirs(os.path.dirname(task.dst_path), exist_ok=True)
                    self._copy_with_progress(task)
                    update_status(task.record_id, "copied")
                    task.status = "copied"
                    logger.info(f"复制完成: {task.src_path} -> {task.dst_path}")
                except Exception as e:
                    task.error_msg = str(e)
                    task.status = "error"
                    update_status(task.record_id, "error", str(e))
                    logger.error(f"复制失败: {task.src_path} - {e}")

                for cb in self._progress_callbacks:
                    try:
                        cb(drive_letter, i + 1, len(tasks), task)
                    except Exception as e:
                        logger.error(f"进度回调错误: {e}")

            for cb in self._done_callbacks:
                try:
                    cb(drive_letter, tasks)
                except Exception as e:
                    logger.error(f"完成回调错误: {e}")

        t = threading.Thread(target=worker, daemon=True)
        with self._lock:
            self._threads[drive_letter] = t
        t.start()
        return tasks

    def _copy_with_progress(self, task: CopyTask, chunk_size: int = 1024 * 1024):
        copied = 0
        with open(task.src_path, "rb") as fsrc:
            with open(task.dst_path, "wb") as fdst:
                while True:
                    chunk = fsrc.read(chunk_size)
                    if not chunk:
                        break
                    fdst.write(chunk)
                    copied += len(chunk)
                    task.progress = copied / task.file_size * 100

    def cancel_copy(self, drive_letter: str):
        with self._lock:
            self._cancel_flags[drive_letter] = True
            t = self._threads.get(drive_letter)
            if t and t.is_alive():
                t.join(timeout=2)

    def get_tasks(self, drive_letter: str) -> List[CopyTask]:
        with self._lock:
            return list(self._tasks.get(drive_letter, []))

    def is_copying(self, drive_letter: str) -> bool:
        with self._lock:
            t = self._threads.get(drive_letter)
            return t is not None and t.is_alive()
