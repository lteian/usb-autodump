# utils/crypto.py
import base64
from cryptography.hazmat.primitives.kdf.pbkdf2 import PBKDF2HMAC
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.backends import default_backend
from cryptography.fernet import Fernet
import threading

_FERNET: Fernet | None = None
_FERNET_LOCK = threading.Lock()

SALT = b"usb_autodump_salt_v1_2024"


def _derive_key(password: str) -> bytes:
    return PBKDF2HMAC(
        algorithm=hashes.SHA256(),
        length=32,
        salt=SALT,
        iterations=100_000,
        backend=default_backend()
    ).derive(password.encode())


def _get_fernet(password: str) -> Fernet:
    key = _derive_key(password)
    return Fernet(base64.urlsafe_b64encode(key))


def encrypt(plaintext: str, password: str) -> str:
    if not plaintext or not password:
        return ""
    try:
        f = _get_fernet(password)
        return f.encrypt(plaintext.encode()).decode()
    except Exception:
        return ""


def decrypt(ciphertext: str, password: str) -> str:
    if not ciphertext or not password:
        return ""
    try:
        f = _get_fernet(password)
        return f.decrypt(ciphertext.encode()).decode()
    except Exception:
        return ""
