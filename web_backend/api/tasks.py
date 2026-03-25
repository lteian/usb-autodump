# api/tasks.py
from fastapi import APIRouter, HTTPException
from models import CopyTaskSchema, TaskProgressSchema
from services import copy_engine_service

router = APIRouter(prefix="/api/tasks", tags=["tasks"])


@router.get("/{drive}")
def get_tasks(drive: str):
    tasks = copy_engine_service.get_tasks(drive)
    copying = copy_engine_service.is_copying(drive)
    total = len(tasks)
    current = sum(1 for t in tasks if t.get("status") == "copying") or (total if copying else 0)
    return TaskProgressSchema(
        drive=drive,
        current=current,
        total=total,
        current_task=CopyTaskSchema(**tasks[current - 1]) if current > 0 and current <= total else None
    )


@router.post("/{drive}/cancel")
def cancel_copy(drive: str):
    copy_engine_service.cancel_copy(drive)
    return {"drive": drive, "cancelled": True}
