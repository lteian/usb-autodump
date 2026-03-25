# main.py
import sys
from pathlib import Path

ROOT = Path(__file__).parent
sys.path.insert(0, str(ROOT))

from PyQt6.QtWidgets import QApplication, QInputDialog, QLineEdit, QMessageBox
from ui.main_window import MainWindow
from utils.logger import setup_logger, get_logger
from utils.config import is_password_set, load_config, get_encryption_password
from utils.crypto import decrypt

setup_logger()
logger = get_logger()

logger.info("=" * 50)
logger.info("U盘自动转储工具启动")
logger.info(f"Python: {sys.version}")
logger.info(f"工作目录: {ROOT}")
logger.info("=" * 50)


def check_encryption_password(parent=None) -> bool:
    """
    如果设置了加密密码，弹出验证对话框。
    返回 True 表示验证通过（或无需验证），False 表示失败要退出。
    """
    cfg = load_config()
    if not is_password_set():
        return True

    # 验证密码
    for attempt in range(3):
        ok = False
        pwd, ok = QInputDialog.getText(
            parent, "🔐 输入密码",
            "请输入加密密码以访问设置：",
            QLineEdit.EchoMode.Password, ""
        )
        if not ok:
            return False  # 用户取消

        if not pwd:
            QMessageBox.warning(parent, "错误", "密码不能为空")
            continue

        stored_encrypted = cfg.get("ftp", {}).get("password", "")
        if stored_encrypted:
            # 解密验证
            decrypted = decrypt(stored_encrypted, pwd)
            if decrypted != "":
                return True
            QMessageBox.warning(parent, "错误", "密码错误，请重试")
        else:
            # 有 encryption_password 但没有 FTP 密码，直接验证
            if get_encryption_password() == pwd:
                return True
            QMessageBox.warning(parent, "错误", "密码错误，请重试")
    return False


def main():
    app = QApplication(sys.argv)
    app.setStyle("Fusion")
    app.setStyleSheet("""
        QToolTip {
            background: #2d2d2d;
            color: #e0e0e0;
            border: 1px solid #4a4a4a;
            border-radius: 4px;
            padding: 4px;
        }
    """)

    # 启动时如果设置了加密密码，先验证
    if not check_encryption_password():
        logger.warning("密码验证失败，退出")
        return

    window = MainWindow()
    window.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
