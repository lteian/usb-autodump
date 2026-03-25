# services/state.py
import sys
import os
from pathlib import Path

# Add the parent directory to path so we can import the original modules
_parent_dir = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(_parent_dir))

from core.usb_monitor import USBMonitor, USBDevice
from core.copy_engine import CopyEngine
from core.ftp_uploader import FTPUploader
from services.ws_manager import ws_manager


class State:
    usb_monitor: USBMonitor
    copy_engine: CopyEngine
    ftp_uploader: FTPUploader
    started: bool = False


state = State()
