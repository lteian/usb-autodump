# -*- coding: utf-8 -*-
"""
Linux USB 模块 - Synology NAS 兼容的 USB 自动存储检测

使用示例:
    from linux_usb import (
        detect_usb_devices, detect_synology_usb,
        USBEventMonitor, create_hotplug_monitor,
        mount_device, sync_and_umount, format_device,
        get_capacity_info, LinuxUSBDevice, FileSystem,
        is_synology, DiskOperationError, MountError, UmountError, FormatError,
    )

    # 检测 USB 设备
    devices = detect_usb_devices()
    for dev in devices:
        print(f"{dev.device_path} @ {dev.mount_point}")

    # 监控热插拔
    def on_usb(action, dev):
        print(f"USB {action}: {dev.device_path}")

    monitor = create_hotplug_monitor(on_usb)
    monitor.start()

兼容: Synology DSM 6.x/7.x, Ubuntu/Debian, UOS, 麒麟Kylin
"""

from usb_detector import (
    LinuxUSBDevice, detect_usb_devices, bytes_to_human,
    get_device_from_path, get_filesystem_type, get_device_capacity,
    get_mount_point, get_device_label, get_uuid,
)

from synology_usb import (
    detect_synology_usb, is_synology, get_synology_volume_path,
    get_usb_external_paths,
)

from disk_operations import (
    FileSystem,
    DiskOperationError, MountError, UmountError, FormatError,
    sync_and_umount, mount_device, format_device,
    get_capacity_info, is_device_busy, list_mounted_usb,
)

from event_monitor import (
    USBEvent, USBEventMonitor, create_hotplug_monitor,
)

__all__ = [
    # 核心数据类
    "LinuxUSBDevice", "USBEvent", "FileSystem",
    # 检测函数
    "detect_usb_devices", "detect_synology_usb", "is_synology",
    "get_synology_volume_path", "get_usb_external_paths",
    # 磁盘操作
    "sync_and_umount", "mount_device", "format_device",
    "get_capacity_info", "is_device_busy", "list_mounted_usb",
    # 事件监控
    "USBEventMonitor", "create_hotplug_monitor",
    # 异常
    "DiskOperationError", "MountError", "UmountError", "FormatError",
    # 工具函数
    "bytes_to_human", "get_device_from_path", "get_filesystem_type",
    "get_device_capacity", "get_mount_point", "get_device_label", "get_uuid",
]
