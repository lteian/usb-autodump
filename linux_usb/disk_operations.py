# -*- coding: utf-8 -*-
"""
磁盘操作 - 挂载/卸载/格式化

兼容: Synology DSM 6.x/7.x, Ubuntu/Debian, UOS, 麒麟Kylin
"""

import os, subprocess
from typing import Dict, Optional
from enum import Enum
from usb_detector import LinuxUSBDevice, run_cmd


class DiskOperationError(Exception): pass
class MountError(DiskOperationError): pass
class UmountError(DiskOperationError): pass
class FormatError(DiskOperationError): pass


class FileSystem(Enum):
    VFAT = "vfat"; EXT4 = "ext4"; NTFS = "ntfs"; EXFAT = "exfat"

    @staticmethod
    def from_string(fs: str) -> Optional["FileSystem"]:
        m = {"vfat": FileSystem.VFAT, "fat32": FileSystem.VFAT, "ext4": FileSystem.EXT4,
             "ntfs": FileSystem.NTFS, "exfat": FileSystem.EXFAT}
        return m.get(fs.lower())


def _run_priv(cmd, timeout=30):
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout, check=False)
        if r.returncode != 0:
            raise DiskOperationError(f"失败: {' '.join(cmd)}\n{r.stderr}")
    except subprocess.TimeoutExpired:
        raise DiskOperationError(f"超时: {' '.join(cmd)}")
    except FileNotFoundError:
        raise DiskOperationError(f"不存在: {cmd[0]}")


def sync_and_umount(device_path: str, force: bool = False) -> bool:
    """安全弹出 USB（sync + umount）"""
    if not os.path.exists(device_path):
        raise UmountError(f"设备不存在: {device_path}")
    try: subprocess.run(["sync"], check=False, timeout=5)
    except: pass
    cmd = ["umount"] + (["-f"] if force else []) + [device_path]
    try:
        _run_priv(cmd); return True
    except DiskOperationError as e:
        raise UmountError(str(e))


def mount_device(device_path: str, mount_point: str, filesystem: str = "") -> bool:
    """挂载设备到指定目录"""
    if not os.path.exists(mount_point):
        try: os.makedirs(mount_point, exist_ok=True)
        except OSError as e: raise MountError(f"无法创建: {mount_point}: {e}")
    cmd = ["mount"] + (["-t", filesystem] if filesystem else []) + [device_path, mount_point]
    try:
        _run_priv(cmd); return True
    except DiskOperationError as e:
        raise MountError(str(e))


def format_device(device_path: str, filesystem: FileSystem, label: str = "") -> bool:
    """格式化设备"""
    if filesystem == FileSystem.VFAT:
        cmd = ["mkfs.vfat"] + (["-n", label] if label else []) + [device_path]
    elif filesystem == FileSystem.EXT4:
        cmd = ["mkfs.ext4"] + (["-L", label] if label else []) + [device_path]
    else:
        raise FormatError(f"不支持: {filesystem.value}")
    try:
        _run_priv(cmd, timeout=60); return True
    except DiskOperationError as e:
        raise FormatError(str(e))


def get_capacity_info(device_path: str) -> Dict[str, int]:
    """获取容量信息 {total, used, free} 字节"""
    info = {"total": 0, "used": 0, "free": 0}
    out = run_cmd(["df", "-B1", device_path])
    if not out: return info
    parts = out.strip().split("\n")
    if len(parts) < 2: return info
    vals = parts[1].split()
    if len(vals) >= 4:
        try: info["total"], info["used"], info["free"] = int(vals[1]), int(vals[2]), int(vals[3])
        except: pass
    return info


def is_device_busy(device_path: str) -> bool:
    """检查设备是否被占用"""
    return bool(run_cmd(["lsof", device_path]))


def list_mounted_usb() -> list:
    """列出已挂载的 USB 设备"""
    devices = []
    try:
        with open("/proc/mounts", "r") as f:
            for line in f:
                parts = line.split()
                if len(parts) >= 3 and parts[2] in ["vfat", "ext4", "ntfs", "exfat", "ext3"]:
                    dev, mp, fs = parts[0], parts[1], parts[2]
                    info = get_capacity_info(dev)
                    devices.append(LinuxUSBDevice(
                        device_path=dev, mount_point=mp, filesystem=fs,
                        total_size=info["total"], free_space=info["free"]))
    except IOError: pass
    return devices


if __name__ == "__main__":
    print("=== 磁盘操作测试 ===")
    print(f"已挂载 USB: {len(list_mounted_usb())} 个")
