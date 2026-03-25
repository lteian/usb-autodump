# models.py
from pydantic import BaseModel
from typing import Optional, List
from datetime import datetime


class USBDeviceSchema(BaseModel):
    drive_letter: str
    label: str
    total_size: int
    free_space: int
    used_space: int
    used_percent: float

    class Config:
        from_attributes = True


class CopyTaskSchema(BaseModel):
    usb_drive: str
    src_path: str
    dst_path: str
    file_size: int
    record_id: int
    progress: float
    status: str
    error_msg: Optional[str] = None

    class Config:
        from_attributes = True


class TaskProgressSchema(BaseModel):
    drive: str
    current: int
    total: int
    current_task: Optional[CopyTaskSchema] = None


class FileRecordSchema(BaseModel):
    id: int
    usb_drive: str
    file_path: str
    local_path: str
    ftp_path: Optional[str] = None
    status: str
    file_size: int
    copied_at: Optional[str] = None
    uploaded_at: Optional[str] = None
    deleted_at: Optional[str] = None
    error_msg: Optional[str] = None

    class Config:
        from_attributes = True


class FTPConfigSchema(BaseModel):
    host: str
    port: int = 21
    username: str
    password: str = ""
    sub_path: str = "/"
    use_tls: bool = False
    max_retry: int = 3


class FTPStatusSchema(BaseModel):
    connected: bool
    queue_size: int


class ConfigSchema(BaseModel):
    local_path: str
    ftp: FTPConfigSchema
    video_extensions: List[str]
    auto_delete_local_after_upload: bool = True
    auto_format_after_copy: bool = False
    max_concurrent_uploads: int = 2
    encryption_password_set: bool = False


class ConfigUpdateSchema(BaseModel):
    local_path: Optional[str] = None
    ftp: Optional[FTPConfigSchema] = None
    video_extensions: Optional[List[str]] = None
    auto_delete_local_after_upload: Optional[bool] = None
    auto_format_after_copy: Optional[bool] = None
    max_concurrent_uploads: Optional[int] = None


class PasswordSetSchema(BaseModel):
    password: str


class FTPTestResultSchema(BaseModel):
    success: bool
    message: str
