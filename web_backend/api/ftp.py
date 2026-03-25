# api/ftp.py
from fastapi import APIRouter
from models import FTPStatusSchema, FTPTestResultSchema
from services import ftp_uploader_service

router = APIRouter(prefix="/api/ftp", tags=["ftp"])


@router.get("/status", response_model=FTPStatusSchema)
def ftp_status():
    status = ftp_uploader_service.get_ftp_status()
    return FTPStatusSchema(**status)


@router.get("/queue")
def upload_queue():
    return ftp_uploader_service.get_upload_queue()


@router.post("/test", response_model=FTPTestResultSchema)
def test_ftp():
    result = ftp_uploader_service.test_ftp_connection()
    return FTPTestResultSchema(**result)
