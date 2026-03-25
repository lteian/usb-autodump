# core/disk_tool.py
import subprocess
import os
from utils.logger import get_logger
from utils.platform_detect import is_windows, is_linux

logger = get_logger()

_win32file = None
_win32con = None

if is_windows():
    try:
        import win32file
        import win32con
        _win32file = win32file
        _win32con = win32con
    except ImportError:
        logger.warning("pywin32 未安装，格式化/弹出功能不可用")


def format_drive(drive_letter: str, fs: str = "FAT32", label: str = "USB") -> bool:
    drive_letter = drive_letter.rstrip("\\/")
    if not drive_letter.endswith((":", "\\")):
        drive_letter += ":"

    if is_windows():
        return _format_windows(drive_letter, fs, label)
    elif is_linux():
        return _format_linux(drive_letter, fs, label)
    logger.error(f"不支持的平台: {drive_letter}")
    return False


def _format_windows(drive_letter: str, fs: str, label: str) -> bool:
    script = f"select volume {drive_letter}\nformat fs={fs} label=\"{label}\" quick\nassign\nexit\n"
    try:
        result = subprocess.run(["diskpart"], input=script, capture_output=True, text=True, timeout=120)
        if result.returncode == 0:
            logger.info(f"格式化成功: {drive_letter} ({fs})")
            return True
        else:
            logger.error(f"格式化失败: {result.stderr or result.stdout}")
            return False
    except subprocess.TimeoutExpired:
        logger.error("格式化超时")
        return False
    except Exception as e:
        logger.error(f"格式化异常: {e}")
        return False


def _format_linux(drive_letter: str, fs: str, label: str) -> bool:
    device = f"/dev/{drive_letter}"
    if not os.path.exists(device):
        logger.error(f"设备不存在: {device}")
        return False
    try:
        if fs.upper() == "FAT32":
            cmd = ["mkfs.vfat", "-n", label, device]
        elif fs.upper() == "NTFS":
            cmd = ["mkfs.ntfs", "-n", label, device]
        elif fs.upper() == "EXT4":
            cmd = ["mkfs.ext4", "-L", label, device]
        else:
            cmd = ["mkfs.vfat", "-n", label, device]
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
        if result.returncode == 0:
            logger.info(f"格式化成功: {device} ({fs})")
            return True
        else:
            logger.error(f"格式化失败: {result.stderr}")
            return False
    except FileNotFoundError:
        logger.error(f"mkfs 工具未找到: {cmd[0]}")
        return False
    except Exception as e:
        logger.error(f"格式化异常: {e}")
        return False


def eject_drive(drive_letter: str) -> bool:
    drive_letter = drive_letter.rstrip("\\/")

    if is_windows():
        if not _win32file or not _win32con:
            logger.error("pywin32 未安装，弹出功能不可用")
            return False
        try:
            vol = f"\\\\.\\{drive_letter}:"
            h = _win32file.CreateFile(
                vol,
                _win32file.GENERIC_READ | _win32file.GENERIC_WRITE,
                _win32file.FILE_SHARE_READ | _win32file.FILE_SHARE_WRITE,
                None,
                _win32file.OPEN_EXISTING,
                _win32file.FILE_ATTRIBUTE_NORMAL,
                None
            )
            _win32file.DeviceIoControl(h, 0x000900a8, None, 0, None)
            _win32file.CloseHandle(h)
            logger.info(f"驱动器已弹出: {drive_letter}")
            return True
        except Exception as e:
            logger.error(f"弹出失败: {drive_letter} - {e}")
            return False

    elif is_linux():
        try:
            result = subprocess.run(["umount", f"/dev/{drive_letter}"],
                                   capture_output=True, text=True, timeout=10)
            if result.returncode == 0:
                logger.info(f"驱动器已卸载: {drive_letter}")
                return True
            else:
                logger.error(f"卸载失败: {result.stderr}")
                return False
        except Exception as e:
            logger.error(f"卸载异常: {e}")
            return False
    return False
