# core/file_record.py
import sqlite3
import threading
from pathlib import Path
from datetime import datetime
from typing import Optional

DB_PATH = Path(__file__).parent.parent / "file_records.db"
_local = threading.local()


def _get_conn() -> sqlite3.Connection:
    if not hasattr(_local, "conn"):
        DB_PATH.parent.mkdir(parents=True, exist_ok=True)
        conn = sqlite3.connect(DB_PATH, check_same_thread=False)
        conn.execute("""
            CREATE TABLE IF NOT EXISTS file_records (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                usb_drive TEXT NOT NULL,
                file_path TEXT NOT NULL,
                local_path TEXT NOT NULL,
                ftp_path TEXT,
                status TEXT NOT NULL DEFAULT 'pending',
                file_size INTEGER DEFAULT 0,
                copied_at DATETIME,
                uploaded_at DATETIME,
                deleted_at DATETIME,
                error_msg TEXT,
                UNIQUE(usb_drive, file_path)
            )
        """)
        conn.execute("CREATE INDEX IF NOT EXISTS idx_status ON file_records(status)")
        conn.commit()
        _local.conn = conn
    return _local.conn


def add_record(usb_drive: str, file_path: str, local_path: str, file_size: int) -> int:
    conn = _get_conn()
    cursor = conn.execute("""
        INSERT OR REPLACE INTO file_records (usb_drive, file_path, local_path, file_size, status, copied_at)
        VALUES (?, ?, ?, ?, 'pending', ?)
    """, (usb_drive, file_path, local_path, file_size, datetime.now().isoformat()))
    conn.commit()
    return cursor.lastrowid


def update_status(record_id: int, status: str, error_msg: Optional[str] = None):
    conn = _get_conn()
    now = datetime.now().isoformat()
    if status == "copied":
        conn.execute("UPDATE file_records SET status=?, copied_at=? WHERE id=?", (status, now, record_id))
    elif status == "uploaded":
        conn.execute("UPDATE file_records SET status=?, uploaded_at=? WHERE id=?", (status, now, record_id))
    elif status == "deleted":
        conn.execute("UPDATE file_records SET status=?, deleted_at=? WHERE id=?", (status, now, record_id))
    elif status == "error":
        conn.execute("UPDATE file_records SET status=?, error_msg=? WHERE id=?", (status, error_msg, record_id))
    else:
        conn.execute("UPDATE file_records SET status=? WHERE id=?", (status, record_id))
    conn.commit()


def update_ftp_path(record_id: int, ftp_path: str):
    conn = _get_conn()
    conn.execute("UPDATE file_records SET ftp_path=? WHERE id=?", (ftp_path, record_id))
    conn.commit()


def get_records_by_status(status: str) -> list:
    conn = _get_conn()
    cursor = conn.execute("SELECT * FROM file_records WHERE status=?", (status,))
    cols = [d[0] for d in cursor.description]
    return [dict(zip(cols, row)) for row in cursor.fetchall()]


def get_all_records() -> list:
    conn = _get_conn()
    cursor = conn.execute("SELECT * FROM file_records ORDER BY id DESC")
    cols = [d[0] for d in cursor.description]
    return [dict(zip(cols, row)) for row in cursor.fetchall()]


def get_pending_count_and_size() -> tuple:
    conn = _get_conn()
    cursor = conn.execute(
        "SELECT COUNT(*), COALESCE(SUM(file_size),0) FROM file_records WHERE status IN ('pending','copying','copied','uploading')"
    )
    row = cursor.fetchone()
    return row[0], row[1]


def get_uploaded_count_and_size() -> tuple:
    conn = _get_conn()
    cursor = conn.execute(
        "SELECT COUNT(*), COALESCE(SUM(file_size),0) FROM file_records WHERE status IN ('uploaded','deleted')"
    )
    row = cursor.fetchone()
    return row[0], row[1]


def delete_record(record_id: int):
    conn = _get_conn()
    conn.execute("DELETE FROM file_records WHERE id=?", (record_id,))
    conn.commit()
