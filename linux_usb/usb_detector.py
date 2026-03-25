# -*- coding: utf-8 -*-
"""
USB 设备检测 - Linux USB 设备通用接口

提供 LinuxUSBDevice 数据类和基础检测功能
兼容: Synology DSM 6.x/7.x, Ubuntu/Debian, UOS, 麒麟Kylin
"""

import os
import re
import subprocess
from dataclasses import dataclass, field
from typing import List, Optional, Dict, Any


@dataclass
class LinuxUSBDevice:
    """Linux USB 存储设备数据类"""
    device_path: str      # /dev/sda1
    mount_point: str = "" # /volume1/usb
    label: str = ""        # U盘标签
    filesystem: str = ""   # ext4/fat32/ntfs
    total_size: int = 0   # 总容量(字节)
    free_space: int = 0   # 可用空间(字节)
    uuid: str = ""
    serial: str = ""

    @property
    def used_space(self) -> int:
        return self.total_size - self.free_space

    @property
    def used_percent(self) -> float:
        if self.total_size == 0:
            return 0.0
        return self.used_space / self.total_size * 100

    @property
    def is_mounted(self) -> bool:
        return bool(self.mount_point)

    @property
    def size_str(self) -> str:
        return bytes_to_human(self.total_size)

    def to_dict(self) -> Dict[str, Any]:
        return {
            "device_path": self.device_path,
            "mount_point": self.mount_point,
            "label": self.label,
            "filesystem": self.filesystem,
            "total_size": self.total_size,
            "free_space": self.free_space,
            "uuid": self.uuid,
            "serial": self.serial,
        }


def bytes_to_human(size: int) -> str:
    """字节数转人类可读格式"""
    for unit in ["B", "KB", "MB", "GB", "TB"]:
        if size < 1024:
            return f"{size:.1f} {unit}"
        size /= 1024
    return f"{size:.1f} PB"


def run_cmd(cmd: List[str], timeout: int = 5) -> Optional[str]:
    """执行系统命令"""
    try:
        result = subprocess.run(cmd, capture_output=True, text=True,
                                timeout=timeout, check=False)
        return result.stdout.strip()
    except Exception:
        return None


def get_filesystem_type(device_path: str) -> str:
    """获取文件系统类型"""
    output = run_cmd(["blkid", "-o", "value", "-s", "TYPE", device_path])
    return output or ""


def get_device_capacity(device_path: str) -> tuple:
    """获取设备容量 (总容量, 可用空间)"""
    output = run_cmd(["df", "-B1", device_path])
    if not output:
        return 0, 0
    lines = output.strip().split("\n")
    if len(lines) < 2:
        return 0, 0
    parts = lines[1].split()
    if len(parts) < 4:
        return 0, 0
    try:
        return int(parts[1]), int(parts[3])
    except (ValueError, IndexError):
        return 0, 0


def get_mount_point(device_path: str) -> str:
    """获取设备挂载点"""
    real_path = os.path.realpath(device_path)
    try:
        with open("/proc/mounts", "r") as f:
            for line in f:
                parts = line.split()
                if len(parts) >= 2 and os.path.realpath(parts[0]) == real_path:
                    return parts[1].replace("\\040", " ")
    except IOError:
        pass
    return ""


def get_device_label(device_path: str) -> str:
    """获取设备卷标"""
    output = run_cmd(["blkid", "-o", "value", "-s", "LABEL", device_path])
    return output or ""


def get_uuid(device_path: str) -> str:
    """获取设备 UUID"""
    try:
        for fname in os.listdir("/dev/disk/by-uuid"):
            link = os.path.join("/dev/disk/by-uuid", fname)
            if os.path.realpath(link) == os.path.realpath(device_path):
                return fname
    except OSError:
        pass
    output = run_cmd(["blkid", "-o", "value", "-s", "UUID", device_path])
    return output or ""


def get_device_from_path(device_path: str) -> Optional[LinuxUSBDevice]:
    """根据设备路径获取完整设备信息"""
    if not os.path.exists(device_path):
        return None
    return LinuxUSBDevice(
        device_path=device_path,
        mount_point=get_mount_point(device_path),
        label=get_device_label(device_path),
        filesystem=get_filesystem_type(device_path),
        total_size=get_device_capacity(device_path)[0],
        free_space=get_device_capacity(device_path)[1],
        uuid=get_uuid(device_path),
    )


def detect_usb_devices() -> List[LinuxUSBDevice]:
    """检测所有 USB 存储设备"""
    devices = []
    
    # 使用 lsblk JSON 输出
    output = run_cmd(["lsblk", "-o", "NAME,SIZE,TYPE,MOUNTPOINT,FSTYPE,UUID,LABEL", "-J", "-b"])
    if output:
        try:
            import json
            data = json.loads(output)
            for block in data.get("blockdevices", []):
                if block.get("type") == "part" and "/usb" in str(block.get("mountpoint", "")):
                    devices.append(LinuxUSBDevice(
                        device_path=f"/dev/{block['name']}",
                        mount_point=block.get("mountpoint", ""),
                        label=block.get("label", ""),
                        filesystem=block.get("fstype", ""),
                        total_size=int(block.get("size", 0)),
                        free_space=0,
                        uuid=block.get("uuid", ""),
                    ))
        except (json.JSONDecodeError, KeyError):
            pass
    
    # 扫描 /dev/disk/by-id/ 找 usb
    try:
        by_id_path = "/dev/disk/by-id"
        if os.path.exists(by_id_path):
            for fname in os.listdir(by_id_path):
                if "usb" in fname.lower() and "-part" in fname:
                    link = os.path.join(by_id_path, fname)
                    real = os.path.realpath(link)
                    if not any(d.device_path == real for d in devices):
                        dev = get_device_from_path(real)
                        if dev:
                            devices.append(dev)
    except OSError:
        pass
    
    # 去重
    seen, unique = set(), []
    for d in devices:
        if d.device_path not in seen:
            seen.add(d.device_path)
            unique.append(d)
    return unique


if __name__ == "__main__":
    print("=== USB 设备检测测试 ===")
    print(f"检测到 {len(detect_usb_devices())} 个 USB 设备")
