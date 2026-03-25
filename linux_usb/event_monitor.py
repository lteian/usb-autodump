# -*- coding: utf-8 -*-
"""
USB 事件监控 - 插入/移除检测

使用 pyudev 监控 USB 设备插入/移除事件
兼容: Synology DSM 6.x/7.x, Ubuntu/Debian, UOS, 麒麟Kylin
"""

import os
import time
import threading
from typing import Callable, Dict, List
from dataclasses import dataclass

try:
    import pyudev
    PYUDEV_AVAILABLE = True
except ImportError:
    PYUDEV_AVAILABLE = False

from usb_detector import LinuxUSBDevice, run_cmd


@dataclass
class USBEvent:
    """USB 事件数据类"""
    action: str  # "add", "remove"
    device: LinuxUSBDevice
    timestamp: float = 0.0


class USBEventMonitor:
    """USB 事件监控器 - 检测 USB 插入/移除事件"""
    
    def __init__(self, callback: Callable[[USBEvent], None] = None):
        """初始化监控器
        
        Args:
            callback: 事件回调函数，签名为 on_event(event: USBEvent)
        """
        self._callback = callback
        self._running = False
        self._thread: threading.Thread = None
        self._known: Dict[str, LinuxUSBDevice] = {}
    
    def start(self):
        """启动监控"""
        if self._running:
            return
        self._running = True
        self._thread = threading.Thread(target=self._loop, daemon=True)
        self._thread.start()
    
    def stop(self):
        """停止监控"""
        self._running = False
        if self._thread:
            self._thread.join(timeout=3)
    
    def _loop(self):
        """监控循环"""
        if PYUDEV_AVAILABLE:
            self._pyudev_loop()
        else:
            self._fallback_loop()
    
    def _pyudev_loop(self):
        """pyudev 实时监控"""
        try:
            context = pyudev.Context()
            monitor = pyudev.Monitor.from_netlink(context)
            monitor.filter_by("usb-storage")
            monitor.filter_by("block")
            
            # 初始化已知设备
            self._known = {d.device_path: d for d in self._get_current_devices()}
            
            while self._running:
                try:
                    device = monitor.poll(timeout=1)
                    if device:
                        self._handle_udev_event(device)
                except Exception:
                    time.sleep(1)
        except Exception:
            self._fallback_loop()
    
    def _fallback_loop(self):
        """轮询备选方案"""
        while self._running:
            current = self._get_current_devices()
            current_paths = {d.device_path for d in current}
            known_paths = set(self._known.keys())
            
            # 新增设备
            for path in current_paths - known_paths:
                dev = next((d for d in current if d.device_path == path), None)
                if dev and self._callback:
                    self._callback(USBEvent("add", dev, time.time()))
            
            # 移除设备
            for path in known_paths - current_paths:
                dev = self._known.get(path)
                if dev and self._callback:
                    self._callback(USBEvent("remove", dev, time.time()))
            
            self._known = {d.device_path: d for d in current}
            time.sleep(1)
    
    def _handle_udev_event(self, pyudev_device):
        """处理 pyudev 事件"""
        action = pyudev_device.action
        device_path = pyudev_device.device_node
        if not device_path:
            return
        
        if action == "add":
            from synology_usb import detect_synology_usb
            devs = detect_synology_usb()
            dev = next((d for d in devs if d.device_path == device_path), None)
            if dev and self._callback:
                self._callback(USBEvent("add", dev, time.time()))
        elif action == "remove":
            dev = self._known.get(device_path)
            if dev and self._callback:
                self._callback(USBEvent("remove", dev, time.time()))
    
    def _get_current_devices(self) -> List[LinuxUSBDevice]:
        """获取当前设备列表"""
        from synology_usb import detect_synology_usb
        return detect_synology_usb()


def create_hotplug_monitor(callback: Callable[[str, LinuxUSBDevice], None]):
    """创建热插拔监控器（简化版）
    
    Args:
        callback: 回调函数，参数为 (action: str, device: LinuxUSBDevice)
                 action 为 "insert" 或 "remove"
    
    Returns:
        USBEventMonitor 实例
    """
    def wrapper(event: USBEvent):
        action = "insert" if event.action == "add" else "remove"
        callback(action, event.device)
    
    return USBEventMonitor(callback=wrapper)


if __name__ == "__main__":
    print("=== USB 事件监控测试 ===")
    print("监控 USB 插入/移除事件 (Ctrl+C 退出)")
    
    def on_event(action: str, dev: LinuxUSBDevice):
        print(f"  [{action.upper()}] {dev.device_path} @ {dev.mount_point or '(未挂载)'}")
    
    monitor = create_hotplug_monitor(on_event)
    monitor.start()
    
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\n停止监控...")
        monitor.stop()
