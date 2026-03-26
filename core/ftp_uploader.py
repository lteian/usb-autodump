# core/ftp_uploader.py
"""
重构后：FTPUploader 变为对 ProcessManager FTP 子进程的代理，
保留原有回调接口，上传工作在子进程中完成。
"""
import multiprocessing as mp
import queue
from typing import Callable, List, Optional
from pathlib import Path
from utils.config import get_ftp_config, load_config
from utils.logger import get_logger

logger = get_logger()


class UploadTask:
    """保持与原有结构兼容"""
    def __init__(self, record_id: int, local_path: str, ftp_sub_path: str, file_size: int):
        self.record_id = record_id
        self.local_path = local_path
        self.ftp_sub_path = ftp_sub_path
        self.file_size = file_size
        self.progress: float = 0.0
        self.status: str = "pending"
        self.error_msg: Optional[str] = None
        self.retry_count: int = 0


class FTPUploader:
    """
    FTPUploader 通过 ProcessManager 的 ftp_worker 子进程执行上传任务，
    保留原有回调接口以维持 UI 兼容性。
    """

    def __init__(self, event_queue: Optional[mp.Queue] = None):
        self._event_queue = event_queue or mp.Queue()
        self._process_manager = None
        self._callbacks: List[Callable] = []
        self._connected = False
        self._queue_size_estimate = 0

    def set_process_manager(self, pm):
        """由 MainWindow 注入 ProcessManager 实例"""
        self._process_manager = pm

    def add_callback(self, cb: Callable):
        self._callbacks.append(cb)

    def start(self):
        """启动 FTP 子进程（由 MainWindow 通过 ProcessManager 统一启动）"""
        logger.info("FTP 上传服务代理已初始化")

    def stop(self):
        """停止 FTP 子进程（由 ProcessManager 统一管理）"""
        pass

    def _connect(self) -> bool:
        """检查 FTP 连接状态"""
        return self._connected

    def _disconnect(self):
        self._connected = False

    def enqueue(self, record_id: int, local_path: str, ftp_sub_path: str, file_size: int):
        """将上传任务加入队列"""
        if self._process_manager:
            self._process_manager.send_ftp_task(record_id, local_path, ftp_sub_path, file_size)
            self._queue_size_estimate += 1

    def get_queue_size(self) -> int:
        """返回队列大小（近似）"""
        if self._process_manager:
            return self._process_manager.get_ftp_queue_size()
        return 0

    def is_connected(self) -> bool:
        return self._connected

    def get_queue(self) -> List[UploadTask]:
        """返回空列表，实际状态通过回调获取"""
        return []

    def poll_events(self):
        """轮询事件队列，处理来自 FTP 子进程的事件（供 MainWindow 调用）"""
        try:
            while True:
                event = self._event_queue.get_nowait()
                self._handle_event(event)
        except queue.Empty:
            pass

    def _handle_event(self, event: dict):
        etype = event.get("type", "")

        if etype == "ftp_status":
            self._connected = event.get("connected", False)

        elif etype == "ftp_success":
            task = UploadTask(
                event["record_id"],
                event["local_path"],
                event.get("ftp_path", ""),
                0
            )
            task.status = "uploaded"
            for cb in self._callbacks:
                try:
                    cb(task)
                except Exception as e:
                    logger.error(f"FTP 上传成功回调错误: {e}")

        elif etype == "ftp_error":
            task = UploadTask(
                event["record_id"],
                event.get("local_path", ""),
                "",
                0
            )
            task.status = "error"
            task.error_msg = event.get("error_msg", "未知错误")
            for cb in self._callbacks:
                try:
                    cb(task)
                except Exception as e:
                    logger.error(f"FTP 上传错误回调错误: {e}")

        elif etype == "ftp_deleted":
            task = UploadTask(
                event["record_id"],
                event.get("local_path", ""),
                "",
                0
            )
            task.status = "deleted"
            for cb in self._callbacks:
                try:
                    cb(task)
                except Exception as e:
                    logger.error(f"FTP 删除回调错误: {e}")

    def get_event_queue(self) -> mp.Queue:
        """返回事件队列，供 MainWindow 轮询"""
        return self._event_queue
