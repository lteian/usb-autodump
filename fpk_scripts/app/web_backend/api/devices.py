# api/devices.py
from fastapi import APIRouter, HTTPException
from models import USBDeviceSchema
from services import usb_monitor_service

router = APIRouter(prefix="/api/devices", tags=["devices"])


@router.get("", response_model=list[USBDeviceSchema])
def list_devices():
    devices = usb_monitor_service.get_current_devices()
    return [USBDeviceSchema(**d) for d in devices]


@router.post("/{drive}/copy")
def start_copy(drive: str):
    from services import copy_engine_service
    if copy_engine_service.is_copying(drive):
        raise HTTPException(status_code=409, detail="该设备正在复制中")
    tasks = copy_engine_service.start_copy(drive)
    from models import CopyTaskSchema
    return {"drive": drive, "total": len(tasks), "tasks": [CopyTaskSchema(**t) for t in tasks]}
