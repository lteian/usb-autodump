# core/ftp_uploader.py
import threading
import time
import ftplib
import os
from pathlib import Path
from typing import Callable, List, Optional
from utils.config import get_ftp_config, load_config
from utils.logger import get_logger
from core.file_record import get_records_by_status, update_status, update_ftp_path

logger = get_logger()


class UploadTask:
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
    def __init__(self):
        self._queue: List[UploadTask] = []
        self._worker_thread: Optional[threading.Thread] = None
        self._running = False
        self._lock = threading.Lock()
        self._callbacks: List[Callable] = []
        self._connected = False
        self._ftp: Optional[ftplib.FTP] = None

    def add_callback(self, cb: Callable):
        self._callbacks.append(cb)

    def start(self):
        with self._lock:
            if self._running:
                return
            self._running = True
        self._worker_thread = threading.Thread(target=self._worker_loop, daemon=True)
        self._worker_thread.start()
        logger.info("FTP 上传服务已启动")

    def stop(self):
        with self._lock:
            self._running = False
        if self._worker_thread:
            self._worker_thread.join(timeout=5)
        self._disconnect()

    def _connect(self) -> bool:
        cfg = get_ftp_config()
        host = cfg.get("host", "")
        port = cfg.get("port", 21)
        username = cfg.get("username", "")
        password = cfg.get("password", "")
        use_tls = cfg.get("use_tls", False)

        if not host:
            logger.warning("FTP 未配置服务器")
            self._connected = False
            return False

        try:
            if use_tls:
                self._ftp = ftplib.FTP_TLS(timeout=10)
                self._ftp.connect(host, port)
                self._ftp.login(username, password)
                self._ftp.prot_p()
            else:
                self._ftp = ftplib.FTP(timeout=10)
                self._ftp.connect(host, port)
                self._ftp.login(username, password)
            self._connected = True
            logger.info(f"FTP 已连接: {host}:{port}")
            return True
        except Exception as e:
            logger.error(f"FTP 连接失败: {e}")
            self._connected = False
            return False

    def _disconnect(self):
        try:
            if self._ftp:
                self._ftp.quit()
        except Exception:
            pass
        self._ftp = None
        self._connected = False

    def _ensure_ftp_dir(self, remote_path: str):
        if not self._ftp:
            return
        parts = remote_path.strip("/").split("/")
        current = ""
        for part in parts:
            current += "/" + part
            try:
                self._ftp.cwd(current)
            except ftplib.error_perm:
                try:
                    self._ftp.mkd(current)
                    self._ftp.cwd(current)
                except ftplib.error_perm:
                    pass

    def _worker_loop(self):
        while self._running:
            if not self._connected:
                if not self._connect():
                    time.sleep(5)
                    continue

            self._load_pending_from_db()

            task = None
            with self._lock:
                if self._queue:
                    task = self._queue.pop(0)

            if task:
                self._upload_file(task)
            else:
                time.sleep(1)

    def _load_pending_from_db(self):
        records = get_records_by_status("copied")
        with self._lock:
            existing = {t.record_id for t in self._queue}
            for rec in records:
                if rec["id"] not in existing:
                    cfg = get_ftp_config()
                    sub = cfg.get("sub_path", "/")
                    remote = f"{sub.rstrip('/')}/{Path(rec['local_path']).name}"
                    task = UploadTask(rec["id"], rec["local_path"], remote, rec["file_size"] or 0)
                    self._queue.append(task)

    def _upload_file(self, task: UploadTask):
        cfg = get_ftp_config()
        max_retry = cfg.get("max_retry", 3)

        if not os.path.exists(task.local_path):
            logger.warning(f"本地文件不存在，跳过: {task.local_path}")
            update_status(task.record_id, "error", "本地文件不存在")
            return

        try:
            self._ensure_ftp_dir(os.path.dirname(task.ftp_sub_path))
            task.status = "uploading"
            dirname = os.path.dirname(task.ftp_sub_path)
            basename = os.path.basename(task.ftp_sub_path)

            with open(task.local_path, "rb") as f:
                if dirname:
                    self._ftp.cwd(dirname)
                self._ftp.storbinary(f"STOR {basename}", f)

            update_status(task.record_id, "uploaded")
            update_ftp_path(task.record_id, task.ftp_sub_path)
            task.status = "uploaded"
            logger.info(f"上传成功: {task.local_path} -> {task.ftp_sub_path}")

            if load_config().get("auto_delete_local_after_upload", True):
                try:
                    os.remove(task.local_path)
                    update_status(task.record_id, "deleted")
                    task.status = "deleted"
                    logger.info(f"本地文件已删除: {task.local_path}")
                except OSError as e:
                    logger.error(f"删除本地文件失败: {task.local_path} - {e}")

        except Exception as e:
            task.retry_count += 1
            task.error_msg = str(e)
            if task.retry_count < max_retry:
                task.status = "pending"
                with self._lock:
                    self._queue.insert(0, task)
                logger.warning(f"上传失败，重试 {task.retry_count}/{max_retry}: {task.local_path} - {e}")
            else:
                task.status = "error"
                update_status(task.record_id, "error", str(e))
                logger.error(f"上传失败，已达最大重试: {task.local_path} - {e}")

        for cb in self._callbacks:
            try:
                cb(task)
            except Exception as e:
                logger.error(f"FTP 回调错误: {e}")

    def enqueue(self, record_id: int, local_path: str, ftp_sub_path: str, file_size: int):
        with self._lock:
            task = UploadTask(record_id, local_path, ftp_sub_path, file_size)
            self._queue.append(task)

    def get_queue_size(self) -> int:
        with self._lock:
            return len(self._queue)

    def is_connected(self) -> bool:
        return self._connected

    def get_queue(self) -> List[UploadTask]:
        with self._lock:
            return list(self._queue)
