# -*- coding: utf-8 -*-
"""
Synology NAS 专用 USB 检测

针对 Synology DSM 6.x/7.x 的 USB 检测
挂载路径: /volumeUSB*, /volume1/usb
"""

import os
import json
from typing import List, Optional

try:
    import pyudev
    PYUDEV_AVAILABLE = True
except ImportError:
    PYUDEV_AVAILABLE = False

from usb_detector import (
    LinuxUSBDevice, run_cmd, get_filesystem_type, 
    get_device_capacity, get_uuid, get_device_label
)


def is_synology() -> bool:
    """检测是否运行在 Synology NAS 上"""
    return os.path.exists("/etc/synoinfo.conf") or os.path.exists("/var/packages/@syno")


def get_synology_volume_path() -> str:
    """获取 Synology USB 挂载基础路径"""
    if os.path.exists("/volume1/usb"):
        return "/volume1/usb"
    for entry in os.listdir("/"):
        if entry.startswith("volumeUSB"):
            return f"/{entry}"
    return ""


def _synology_get_mount_point(device_path: str) -> str:
    """获取 Synology 设备挂载点"""
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


def synology_get_usb_devices_pyudev() -> List[LinuxUSBDevice]:
    """使用 pyudev 获取 Synology USB 设备"""
    if not PYUDEV_AVAILABLE:
        return []
    
    devices = []
    try:
        context = pyudev.Context()
        for device in context.list_devices(subsystem="block", DEVTYPE="partition"):
            parent = device.find_parent("usb")
            if not parent:
                continue
            device_path = device.device_node
            if not device_path:
                continue
            
            serial = device.get("ID_SERIAL_SHORT", "") or device.get("ID_SERIAL", "")
            mount_point = _synology_get_mount_point(device_path)
            total, free = get_device_capacity(device_path)
            fs_type = device.get("ID_FS_TYPE", "") or get_filesystem_type(device_path)
            uuid_val = device.get("ID_FS_UUID", "") or get_uuid(device_path)
            label = device.get("ID_FS_LABEL", "") or get_device_label(device_path)
            
            devices.append(LinuxUSBDevice(
                device_path=device_path,
                mount_point=mount_point,
                label=label,
                filesystem=fs_type,
                total_size=total,
                free_space=free,
                uuid=uuid_val,
                serial=serial,
            ))
    except Exception:
        pass
    return devices


def synology_get_usb_devices_fallback() -> List[LinuxUSBDevice]:
    """不使用 pyudev 的检测方案"""
    devices = []
    try:
        with open("/proc/mounts", "r") as f:
            for line in f:
                parts = line.split()
                if len(parts) < 3:
                    continue
                device_path, mount_point = parts[0], parts[1]
                if not (mount_point.startswith("/volumeUSB") or 
                        mount_point.startswith("/volume1/usb")):
                    continue
                total, free = get_device_capacity(device_path)
                devices.append(LinuxUSBDevice(
                    device_path=device_path,
                    mount_point=mount_point,
                    label=get_device_label(device_path),
                    filesystem=get_filesystem_type(device_path),
                    total_size=total,
                    free_space=free,
                    uuid=get_uuid(device_path),
                ))
    except IOError:
        pass
    return devices


def detect_synology_usb() -> List[LinuxUSBDevice]:
    """检测 Synology USB 设备统一入口"""
    if is_synology() and PYUDEV_AVAILABLE:
        return synology_get_usb_devices_pyudev()
    return synology_get_usb_devices_fallback()


def get_usb_external_paths() -> List[str]:
    """获取所有 Synology USB 挂载路径"""
    paths = []
    if os.path.exists("/volume1/usb"):
        paths.append("/volume1/usb")
    try:
        for entry in os.listdir("/"):
            if entry.startswith("volumeUSB"):
                paths.append(f"/{entry}")
    except OSError:
        pass
    return paths


if __name__ == "__main__":
    print(f"Synology: {'是' if is_synology() else '否'}")
    print(f"pyudev: {'可用' if PYUDEV_AVAILABLE else '不可用'}")
    print(f"USB 路径: {get_usb_external_paths()}")
    print(f"检测到 {len(detect_synology_usb())} 个设备")
