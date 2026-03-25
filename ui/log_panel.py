# ui/log_panel.py
from PyQt6.QtWidgets import QTextEdit
from PyQt6.QtGui import QTextCursor
from datetime import datetime


class LogPanel(QTextEdit):
    COLOR_MAP = {
        "INFO": "#4CAF50",
        "WARNING": "#FF9800",
        "ERROR": "#F44336",
        "DEBUG": "#9E9E9E",
    }

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setReadOnly(True)
        self.setMaximumHeight(160)
        self.setStyleSheet("""
            QTextEdit {
                background: #1e1e1e;
                color: #d4d4d4;
                font-family: 'Consolas', 'Courier New', monospace;
                font-size: 12px;
                border: 1px solid #3c3c3c;
                border-radius: 4px;
            }
        """)

    def append_log(self, level: str, message: str):
        color = self.COLOR_MAP.get(level.upper(), "#d4d4d4")
        ts = datetime.now().strftime("%H:%M:%S")
        html = (
            f'<span style="color: #808080;">[{ts}]</span> '
            f'<span style="color: {color};">{level.upper()}</span> '
            f'<span style="color: #d4d4d4;">{message}</span>'
        )
        self.append(html)
        self.moveCursor(QTextCursor.MoveOperation.End)

    def append_info(self, message: str):
        self.append_log("INFO", message)

    def append_warning(self, message: str):
        self.append_log("WARNING", message)

    def append_error(self, message: str):
        self.append_log("ERROR", message)

    def append_debug(self, message: str):
        self.append_log("DEBUG", message)

    def clear_logs(self):
        self.clear()
