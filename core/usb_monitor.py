# core/usb_monitor.py
import threading
import time
from typing import Callable, Optional, List, Dict
from utils.platform_detect import is_windows, is_linux
from utils.logger import get_logger

logger = get_logger()

# 条件导入
_wmi_client = None
_pywintypes = None
_pyudev = None

if is_windows():
    try:
        import win32com.client
        import pywintypes
        _wmi_client = win32com.client
        _pywintypes = pywintypes
    except ImportError:
        logger.warning("pywin32 未安装，USB 检测不可用")
elif is_linux():
    try:
        import pyudev
        _pyudev = pyudev
    except ImportError:
        logger.warning("pyudev 未安装，USB 检测不可用")


class USBDevice:
    def __init__(self, drive_letter: str, label: str, total_size: int, free_space: int):
        self.drive_letter = drive_letter
        self.label = label
        self.total_size = total_size
        self.free_space = free_space
        self.used_space = total_size - free_space

    @property
    def used_percent(self) -> float:
        if self.total_size == 0:
            return 0.0
        return self.used_space / self.total_size * 100

    def __repr__(self):
        return f"USBDevice({self.drive_letter}, {self.label}, {self.total_size} bytes)"


class USBMonitor:
    def __init__(self):
        self._callbacks: List[Callable] = []
        self._running = False
        self._thread: Optional[threading.Thread] = None
        self._last_drives: Dict[str, USBDevice] = {}

    def add_callback(self, cb: Callable):
        self._callbacks.append(cb)

    def start(self):
        if self._running:
            return
        self._running = True
        self._thread = threading.Thread(target=self._poll_loop, daemon=True)
        self._thread.start()
        logger.info("USB 监控已启动")

    def stop(self):
        self._running = False
        if self._thread:
            self._thread.join(timeout=3)
        logger.info("USB 监控已停止")

    def _poll_loop(self):
        if is_windows():
            self._windows_poll()
        elif is_linux() and _pyudev:
            self._linux_poll()

    def _windows_poll(self):
        if not _wmi_client:
            return
        wmi = _wmi_client.Dispatch("WbemScripting.SWbemLocator")
        swbem = wmi.ConnectServer(".", "root\\cimv2")

        while self._running:
            try:
                current = self._get_windows_usb_drives(swbem)
                current_keys = set(current.keys())
                last_keys = set(self._last_drives.keys())

                for dk in current_keys - last_keys:
                    logger.info(f"USB 插入: {dk}")
                    for cb in self._callbacks:
                        try:
                            cb("insert", current[dk])
                        except Exception as e:
                            logger.error(f"USB 回调错误(insert): {e}")

                for dk in last_keys - current_keys:
                    logger.info(f"USB 移除: {dk}")
                    for cb in self._callbacks:
                        try:
                            cb("remove", self._last_drives[dk])
                        except Exception as e:
                            logger.error(f"USB 回调错误(remove): {e}")

                self._last_drives = current
            except Exception as e:
                logger.error(f"USB 轮询错误: {e}")

            time.sleep(1)

    def _get_windows_usb_drives(self, swbem) -> Dict[str, USBDevice]:
        drives = {}
        try:
            for disk in swbem.ExecQuery("SELECT * FROM Win32_LogicalDisk WHERE DriveType=2"):
                try:
                    dl = disk.DeviceID
                    label = disk.VolumeName or "U盘"
                    total = int(disk.Size) if disk.Size else 0
                    free = int(disk.FreeSpace) if disk.FreeSpace else 0
                    drives[dl] = USBDevice(dl, label, total, free)
                except Exception:
                    pass
        except Exception as e:
            logger.error(f"查询 USB 设备失败: {e}")
        return drives

    def _linux_poll(self):
        if not _pyudev:
            return
        context = _pyudev.Context()
        monitor = _pyudev.Monitor.from_netlink(context)
        monitor.filter_by("usb-storage")
        while self._running:
            try:
                device = monitor.poll(timeout=1)
                if device:
                    logger.info(f"USB 事件: {device.action} - {device.device_node}")
            except Exception as e:
                logger.error(f"Linux USB 监控错误: {e}")
                time.sleep(1)

    def get_current_devices(self) -> List[USBDevice]:
        if is_windows() and _wmi_client:
            wmi = _wmi_client.Dispatch("WbemScripting.SWbemLocator")
            swbem = wmi.ConnectServer(".", "root\\cimv2")
            return list(self._get_windows_usb_drives(swbem).values())
        return []
