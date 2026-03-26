# core/copy_engine.py
"""
重构后：CopyEngine 变为对 ProcessManager 的代理，
保留原有回调接口，实际复制工作在子进程中完成。
"""
import multiprocessing as mp
import queue
from pathlib import Path
from typing import Callable, List, Optional
from utils.config import get_local_path
from utils.logger import get_logger

logger = get_logger()


class CopyTask:
    """保持与原有结构兼容"""
    def __init__(self, drive_letter: str, src_path: str, dst_path: str, file_size: int, record_id: int):
        self.drive_letter = drive_letter
        self.src_path = src_path
        self.dst_path = dst_path
        self.file_size = file_size
        self.record_id = record_id
        self.progress: float = 0.0
        self.status: str = "pending"
        self.error_msg: Optional[str] = None


class CopyEngine:
    """
    CopyEngine 通过 ProcessManager 启动子进程执行复制任务，
    保留原有回调接口以维持 UI 兼容性。
    """

    def __init__(self, event_queue: Optional[mp.Queue] = None):
        self._event_queue = event_queue or mp.Queue()
        # 内部维护一个简单的 ProcessManager 引用（延迟初始化）
        self._process_manager = None
        self._progress_callbacks: List[Callable] = []
        self._done_callbacks: List[Callable] = []
        self._scan_callbacks: List[Callable] = []
        self._scan_done_callbacks: List[Callable] = []

    def set_process_manager(self, pm):
        """由 MainWindow 注入 ProcessManager 实例"""
        self._process_manager = pm

    def add_progress_callback(self, cb: Callable):
        self._progress_callbacks.append(cb)

    def add_done_callback(self, cb: Callable):
        self._done_callbacks.append(cb)

    def add_scan_callback(self, cb: Callable):
        """扫描开始回调"""
        self._scan_callbacks.append(cb)

    def add_scan_done_callback(self, cb: Callable):
        """扫描完成回调"""
        self._scan_done_callbacks.append(cb)

    def start_copy(self, drive_letter: str) -> List[CopyTask]:
        """启动 U 盘复制子进程"""
        if not self._process_manager:
            logger.error("ProcessManager 未设置")
            return []

        # 检查是否已有该盘符的复制进程
        if self._process_manager.is_copy_worker_alive(drive_letter):
            logger.warning(f"复制进程已存在: {drive_letter}")
            return []

        local_base = Path(get_local_path(drive_letter))
        dest_dir = str(local_base / drive_letter.rstrip("\\/"))

        for cb in self._scan_callbacks:
            try:
                cb(drive_letter)
            except Exception as e:
                logger.error(f"扫描开始回调错误: {e}")

        self._process_manager.start_copy_worker(drive_letter, dest_dir)
        return []

    def cancel_copy(self, drive_letter: str):
        """停止指定 U 盘的复制子进程"""
        if self._process_manager:
            self._process_manager.stop_copy_worker(drive_letter)

    def is_copying(self, drive_letter: str) -> bool:
        """检查是否正在复制"""
        if self._process_manager:
            return self._process_manager.is_copy_worker_alive(drive_letter)
        return False

    def get_tasks(self, drive_letter: str) -> List[CopyTask]:
        """返回空列表，实际任务状态通过回调获取"""
        return []

    def poll_events(self):
        """轮询事件队列，处理来自子进程的事件（供 MainWindow 调用）"""
        try:
            while True:
                event = self._event_queue.get_nowait()
                self._handle_event(event)
        except queue.Empty:
            pass

    def _handle_event(self, event: dict):
        etype = event.get("type", "")

        if etype == "copy_scan_start":
            for cb in self._scan_callbacks:
                try:
                    cb(event["drive_letter"])
                except Exception as e:
                    logger.error(f"扫描开始回调错误: {e}")

        elif etype == "copy_scan_done":
            for cb in self._scan_done_callbacks:
                try:
                    cb(event["drive_letter"], event["total"])
                except Exception as e:
                    logger.error(f"扫描完成回调错误: {e}")

        elif etype == "copy_progress":
            task = CopyTask(
                event["drive_letter"],
                event.get("src_path", ""),
                event.get("dst_path", ""),
                0,
                event.get("record_id", 0)
            )
            task.progress = event.get("progress", 0)
            task.status = event.get("status", "copying")
            for cb in self._progress_callbacks:
                try:
                    cb(event["drive_letter"], event["idx"], event["total"], task)
                except Exception as e:
                    logger.error(f"进度回调错误: {e}")

        elif etype == "copy_done":
            for cb in self._done_callbacks:
                try:
                    cb(event["drive_letter"], event.get("total", 0), event.get("copied", 0), event.get("errors", 0))
                except Exception as e:
                    logger.error(f"完成回调错误: {e}")

        elif etype == "copy_error":
            task = CopyTask(
                event["drive_letter"],
                event.get("src_path", ""),
                "",
                0,
                event.get("record_id", 0)
            )
            task.status = "error"
            task.error_msg = event.get("error_msg", "未知错误")
            for cb in self._progress_callbacks:
                try:
                    cb(event["drive_letter"], event.get("idx", 0), event.get("total", 0), task)
                except Exception as e:
                    logger.error(f"错误回调错误: {e}")

    def get_event_queue(self) -> mp.Queue:
        """返回事件队列，供 MainWindow 轮询"""
        return self._event_queue
