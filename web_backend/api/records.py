# api/records.py
from fastapi import APIRouter, HTTPException
from typing import List
import sys
from pathlib import Path

_parent = Path(__file__).resolve().parent.parent.parent
if str(_parent) not in sys.path:
    sys.path.insert(0, str(_parent))

from core.file_record import get_all_records, delete_record
from models import FileRecordSchema

router = APIRouter(prefix="/api/records", tags=["records"])


@router.get("", response_model=List[FileRecordSchema])
def list_records():
    records = get_all_records()
    return [FileRecordSchema(**r) for r in records]


@router.delete("/{record_id}")
def remove_record(record_id: int):
    delete_record(record_id)
    return {"id": record_id, "deleted": True}
