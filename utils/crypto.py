# utils/crypto.py
import base64
from cryptography.hazmat.primitives.kdf.pbkdf2 import PBKDF2HMAC
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.backends import default_backend
from cryptography.fernet import Fernet

_FERNET: Fernet | None = None
SALT = b"usb_autodump_salt_v1_2024"
PASSWORD = "xajwd@30423"


def _get_fernet() -> Fernet:
    global _FERNET
    if _FERNET is None:
        key = PBKDF2HMAC(
            algorithm=hashes.SHA256(),
            length=32,
            salt=SALT,
            iterations=100_000,
            backend=default_backend()
        ).derive(PASSWORD.encode())
        _FERNET = Fernet(base64.urlsafe_b64encode(key))
    return _FERNET


def encrypt(plaintext: str) -> str:
    if not plaintext:
        return ""
    try:
        return _get_fernet().encrypt(plaintext.encode()).decode()
    except Exception:
        return ""


def decrypt(ciphertext: str) -> str:
    if not ciphertext:
        return ""
    try:
        return _get_fernet().decrypt(ciphertext.encode()).decode()
    except Exception:
        return ""
