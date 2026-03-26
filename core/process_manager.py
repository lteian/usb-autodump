# core/process_manager.py
import multiprocessing as mp
import queue
import time
import traceback
from typing import Dict, Optional, Callable
from utils.logger import get_logger

logger = get_logger()


class ProcessManager:
    """管理所有子进程的生命周期"""

    def __init__(self, event_queue: mp.Queue):
        self._event_queue = event_queue
        self._copy_workers: Dict[str, mp.Process] = {}
        self._ftp_process: Optional[mp.Process] = None
        self._ftp_task_queue: Optional[mp.Queue] = None
        self._running = False

    def start_ftp_worker(self):
        """启动 FTP 上传子进程"""
        if self._ftp_process and self._ftp_process.is_alive():
            logger.warning("FTP 进程已在运行")
            return

        self._ftp_task_queue = mp.Queue()
        self._ftp_process = mp.Process(
            target=ftp_worker,
            args=(self._ftp_task_queue, self._event_queue),
            daemon=True,
            name="FTPWorker"
        )
        self._ftp_process.start()
        logger.info("FTP 子进程已启动")

    def stop_ftp_worker(self):
        """停止 FTP 上传子进程"""
        if self._ftp_process and self._ftp_process.is_alive():
            self._ftp_process.terminate()
            self._ftp_process.join(timeout=5)
            if self._ftp_process.is_alive():
                self._ftp_process.kill()
            logger.info("FTP 子进程已停止")
        self._ftp_process = None
        self._ftp_task_queue = None

    def start_copy_worker(self, drive_letter: str, dest_dir: str) -> bool:
        """启动 U 盘复制子进程"""
        if drive_letter in self._copy_workers and self._copy_workers[drive_letter].is_alive():
            logger.warning(f"复制进程已存在: {drive_letter}")
            return False

        task_queue = mp.Queue()
        proc = mp.Process(
            target=copy_worker,
            args=(drive_letter, dest_dir, task_queue, self._event_queue),
            daemon=True,
            name=f"CopyWorker-{drive_letter}"
        )
        proc.start()
        self._copy_workers[drive_letter] = proc
        logger.info(f"复制子进程已启动: {drive_letter}")
        return True

    def stop_copy_worker(self, drive_letter: str):
        """停止指定 U 盘的复制子进程"""
        proc = self._copy_workers.get(drive_letter)
        if proc:
            if proc.is_alive():
                proc.terminate()
                proc.join(timeout=3)
                if proc.is_alive():
                    proc.kill()
            del self._copy_workers[drive_letter]
            logger.info(f"复制子进程已停止: {drive_letter}")

    def stop_all(self):
        """停止所有子进程"""
        self._running = False
        for drive_letter in list(self._copy_workers.keys()):
            self.stop_copy_worker(drive_letter)
        self.stop_ftp_worker()
        logger.info("所有子进程已停止")

    def is_copy_worker_alive(self, drive_letter: str) -> bool:
        """检查复制进程是否存活"""
        proc = self._copy_workers.get(drive_letter)
        return proc is not None and proc.is_alive()

    def get_copy_worker_drive(self) -> Optional[str]:
        """获取当前存活的复制进程对应的盘符"""
        for drive_letter, proc in self._copy_workers.items():
            if proc.is_alive():
                return drive_letter
        return None

    def send_ftp_task(self, record_id: int, local_path: str, ftp_sub_path: str, file_size: int):
        """向 FTP 进程发送上传任务"""
        if self._ftp_task_queue:
            try:
                self._ftp_task_queue.put_nowait({
                    "record_id": record_id,
                    "local_path": local_path,
                    "ftp_sub_path": ftp_sub_path,
                    "file_size": file_size
                })
            except queue.Full:
                logger.error("FTP 任务队列已满")

    def get_ftp_queue_size(self) -> int:
        """获取 FTP 任务队列大小（近似）"""
        if self._ftp_task_queue:
            return self._ftp_task_queue.qsize()
        return 0


def copy_worker(drive_letter: str, dest_dir: str, task_queue: mp.Queue, event_queue: mp.Queue):
    """U 盘子进程：扫描并复制视频文件"""
    try:
        import os
        import sys
        from pathlib import Path

        # 将 core 目录加入 path 以便导入
        core_dir = Path(__file__).parent.parent / "core"
        if str(core_dir) not in sys.path:
            sys.path.insert(0, str(core_dir))

        from utils.config import get_video_extensions
        from core.file_record import add_record, update_status

        video_exts = get_video_extensions()
        tasks = []

        # 阶段1：扫描视频文件
        event_queue.put({
            "type": "copy_scan_start",
            "drive_letter": drive_letter
        })

        for root, dirs, files in os.walk(drive_letter):
            for filename in files:
                ext = Path(filename).suffix.lower()
                if ext not in video_exts:
                    continue
                src_path = os.path.join(root, filename)
                rel_path = os.path.relpath(src_path, drive_letter)
                dst_path = str(Path(dest_dir) / rel_path)
                try:
                    file_size = os.path.getsize(src_path)
                except OSError:
                    continue
                record_id = add_record(drive_letter, src_path, dst_path, file_size)
                tasks.append({
                    "record_id": record_id,
                    "src_path": src_path,
                    "dst_path": dst_path,
                    "file_size": file_size
                })

        event_queue.put({
            "type": "copy_scan_done",
            "drive_letter": drive_letter,
            "total": len(tasks)
        })

        # 阶段2：复制文件
        for i, task in enumerate(tasks):
            try:
                update_status(task["record_id"], "copying")
                os.makedirs(os.path.dirname(task["dst_path"]), exist_ok=True)
                _copy_with_progress(task["src_path"], task["dst_path"], task["file_size"])
                update_status(task["record_id"], "copied")
                event_queue.put({
                    "type": "copy_progress",
                    "drive_letter": drive_letter,
                    "idx": i + 1,
                    "total": len(tasks),
                    "record_id": task["record_id"],
                    "src_path": task["src_path"],
                    "dst_path": task["dst_path"],
                    "progress": 100.0,
                    "status": "copied"
                })
            except Exception as e:
                update_status(task["record_id"], "error", str(e))
                event_queue.put({
                    "type": "copy_error",
                    "drive_letter": drive_letter,
                    "idx": i + 1,
                    "total": len(tasks),
                    "record_id": task["record_id"],
                    "src_path": task["src_path"],
                    "error_msg": str(e)
                })

        # 复制完成
        copied = sum(1 for _ in tasks)
        event_queue.put({
            "type": "copy_done",
            "drive_letter": drive_letter,
            "total": len(tasks),
            "copied": copied,
            "errors": 0
        })

    except Exception as e:
        event_queue.put({
            "type": "copy_error",
            "drive_letter": drive_letter,
            "error_msg": f"复制进程异常: {e}",
            "traceback": traceback.format_exc()
        })


def _copy_with_progress(src_path: str, dst_path: str, file_size: int, chunk_size: int = 1024 * 1024):
    """复制文件并定期 fsync"""
    import os
    copied = 0
    chunk_count = 0
    with open(src_path, "rb") as fsrc:
        with open(dst_path, "wb") as fdst:
            while True:
                chunk = fsrc.read(chunk_size)
                if not chunk:
                    break
                fdst.write(chunk)
                copied += len(chunk)
                chunk_count += 1
                # 每 8MB 做一次 fsync
                if len(chunk) == chunk_size and chunk_count % 8 == 0:
                    try:
                        os.fsync(fdst.fileno())
                    except Exception:
                        pass
            # 最终刷盘
            try:
                fdst.flush()
                os.fsync(fdst.fileno())
            except Exception:
                pass


def ftp_worker(task_queue: mp.Queue, event_queue: mp.Queue):
    """FTP 上传子进程：从队列接收任务并上传"""
    try:
        import ftplib
        import os
        from pathlib import Path
        import sys

        core_dir = Path(__file__).parent.parent / "core"
        if str(core_dir) not in sys.path:
            sys.path.insert(0, str(core_dir))

        from utils.config import get_ftp_config, load_config
        from core.file_record import update_status, update_ftp_path

        def connect() -> Optional[ftplib.FTP]:
            cfg = get_ftp_config()
            host = cfg.get("host", "")
            port = cfg.get("port", 21)
            username = cfg.get("username", "")
            password = cfg.get("password", "")
            use_tls = cfg.get("use_tls", False)

            if not host:
                return None

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
                return ftp
            except Exception:
                return None

        def ensure_dir(ftp, remote_path: str):
            parts = remote_path.strip("/").split("/")
            current = ""
            for part in parts:
                current += "/" + part
                try:
                    ftp.cwd(current)
                except ftplib.error_perm:
                    try:
                        ftp.mkd(current)
                        ftp.cwd(current)
                    except ftplib.error_perm:
                        pass

        ftp = connect()
        if not ftp:
            event_queue.put({"type": "ftp_status", "connected": False})
            # 重试连接
            import time
            while True:
                time.sleep(5)
                ftp = connect()
                if ftp:
                    event_queue.put({"type": "ftp_status", "connected": True})
                    break

        event_queue.put({"type": "ftp_status", "connected": True})

        while True:
            try:
                task = task_queue.get(timeout=1)
            except queue.Empty:
                # 尝试重连
                if not ftp:
                    ftp = connect()
                    if ftp:
                        event_queue.put({"type": "ftp_status", "connected": True})
                continue

            record_id = task["record_id"]
            local_path = task["local_path"]
            ftp_sub_path = task["ftp_sub_path"]
            file_size = task["file_size"]
            cfg = get_ftp_config()
            max_retry = cfg.get("max_retry", 3)
            retry_count = 0

            while retry_count < max_retry:
                if not os.path.exists(local_path):
                    update_status(record_id, "error", "本地文件不存在")
                    event_queue.put({
                        "type": "ftp_error",
                        "record_id": record_id,
                        "local_path": local_path,
                        "error_msg": "本地文件不存在"
                    })
                    break

                try:
                    if not ftp:
                        ftp = connect()
                        if not ftp:
                            raise Exception("FTP 连接失败")

                    ensure_dir(ftp, os.path.dirname(ftp_sub_path))
                    dirname = os.path.dirname(ftp_sub_path)
                    basename = os.path.basename(ftp_sub_path)

                    if dirname:
                        ftp.cwd(dirname)

                    with open(local_path, "rb") as f:
                        ftp.storbinary(f"STOR {basename}", f)

                    update_status(record_id, "uploaded")
                    update_ftp_path(record_id, ftp_sub_path)
                    event_queue.put({
                        "type": "ftp_success",
                        "record_id": record_id,
                        "local_path": local_path,
                        "ftp_path": ftp_sub_path
                    })
                    logger.info(f"上传成功: {local_path} -> {ftp_sub_path}")

                    # auto_delete_local_after_upload
                    if load_config().get("auto_delete_local_after_upload", True):
                        try:
                            os.remove(local_path)
                            update_status(record_id, "deleted")
                            event_queue.put({
                                "type": "ftp_deleted",
                                "record_id": record_id,
                                "local_path": local_path
                            })
                        except OSError as e:
                            logger.error(f"删除本地文件失败: {local_path} - {e}")

                    break

                except Exception as e:
                    retry_count += 1
                    if retry_count < max_retry:
                        logger.warning(f"上传失败，重试 {retry_count}/{max_retry}: {local_path} - {e}")
                        ftp = None  # 重连
                    else:
                        update_status(record_id, "error", str(e))
                        event_queue.put({
                            "type": "ftp_error",
                            "record_id": record_id,
                            "local_path": local_path,
                            "error_msg": str(e)
                        })

    except Exception as e:
        event_queue.put({
            "type": "ftp_error",
            "error_msg": f"FTP 进程异常: {e}",
            "traceback": traceback.format_exc()
        })
