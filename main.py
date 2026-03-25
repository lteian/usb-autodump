# main.py
import sys
from pathlib import Path

ROOT = Path(__file__).parent
sys.path.insert(0, str(ROOT))

from PyQt6.QtWidgets import QApplication
from ui.main_window import MainWindow
from utils.logger import setup_logger, get_logger

setup_logger()
logger = get_logger()

logger.info("=" * 50)
logger.info("U盘自动转储工具启动")
logger.info(f"Python: {sys.version}")
logger.info(f"工作目录: {ROOT}")
logger.info("=" * 50)


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

    window = MainWindow()
    window.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
