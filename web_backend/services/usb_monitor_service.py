# services/usb_monitor_service.py
import asyncio
from core.usb_monitor import USBMonitor, USBDevice
from core.disk_tool import eject_drive
from services.ws_manager import ws_manager
from services.state import state
import sys
import os
from pathlib import Path

# Ensure parent modules are importable
_parent = Path(__file__).resolve().parent.parent.parent
if str(_parent) not in sys.path:
    sys.path.insert(0, str(_parent))


def _make_device_dict(dev: USBDevice) -> dict:
    return {
        "drive_letter": dev.drive_letter,
        "label": dev.label,
        "total_size": dev.total_size,
        "free_space": dev.free_space,
        "used_space": dev.used_space,
        "used_percent": dev.used_percent,
    }


def start_usb_monitor():
    monitor = USBMonitor()

    def on_usb_event(action: str, device: USBDevice):
        asyncio.run(ws_manager.broadcast_device({
            "action": action,
            "device": _make_device_dict(device)
        }))

    monitor.add_callback(on_usb_event)
    monitor.start()
    state.usb_monitor = monitor
    return monitor


def get_current_devices() -> list:
    monitor: USBMonitor = getattr(state, "usb_monitor", None)
    if not monitor:
        return []
    return [_make_device_dict(d) for d in monitor.get_current_devices()]
