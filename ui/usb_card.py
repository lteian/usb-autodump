# ui/usb_card.py
from PyQt6.QtWidgets import QWidget, QVBoxLayout, QHBoxLayout, QLabel, QProgressBar, QPushButton, QFrame
from PyQt6.QtCore import Qt, pyqtSignal
from PyQt6.QtGui import QPainter, QColor, QPen, QFont


class PieChart(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMinimumSize(80, 80)
        self._percent = 0.0

    def set_percent(self, value: float):
        self._percent = max(0.0, min(100.0, value))
        self.update()

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        w, h = self.width(), self.height()
        side = min(w, h)
        x, y = (w - side) / 2, (h - side) / 2

        painter.setPen(Qt.PenStyle.NoPen)
        painter.setBrush(QColor("#3c3c3c"))
        painter.drawEllipse(int(x), int(y), int(side), int(side))

        if self._percent > 0:
            painter.setBrush(QColor("#FF9800"))
            span = int(self._percent * 360 / 100)
            painter.drawPie(int(x), int(y), int(side), int(side), 90 * 16, -span * 16)

        painter.setPen(QPen(QColor("#666666"), 2))
        painter.setBrush(Qt.BrushStyle.NoBrush)
        painter.drawEllipse(int(x), int(y), int(side), int(side))

        painter.setPen(QColor("#ffffff"))
        painter.setFont(QFont("Arial", 10, QFont.Weight.Bold))
        painter.drawText(self.rect(), Qt.AlignmentFlag.AlignCenter, f"{self._percent:.0f}%")


class USBCard(QFrame):
    format_clicked = pyqtSignal(str)
    eject_clicked = pyqtSignal(str)
    cancel_clicked = pyqtSignal(str)

    STATUS_IDLE = "idle"
    STATUS_COPYING = "copying"
    STATUS_DONE = "done"
    STATUS_FORMATTING = "formatting"

    def __init__(self, parent=None):
        super().__init__(parent)
        self._drive_letter = ""
        self._status = self.STATUS_IDLE
        self._setup_ui()

    def _setup_ui(self):
        self.setFrameStyle(QFrame.Shape.StyledPanel | QFrame.Shadow.Raised)
        self.setStyleSheet("""
            QFrame {
                background: #2d2d2d;
                border: 1px solid #4a4a4a;
                border-radius: 8px;
                padding: 8px;
                min-width: 160px;
            }
        """)
        layout = QVBoxLayout(self)
        layout.setSpacing(6)

        # 标题
        header = QHBoxLayout()
        self.drive_label = QLabel("空闲")
        self.drive_label.setStyleSheet("color: #ffffff; font-size: 16px; font-weight: bold;")
        self.status_label = QLabel("")
        self.status_label.setStyleSheet("color: #9e9e9e; font-size: 11px;")
        header.addWidget(self.drive_label)
        header.addStretch()
        header.addWidget(self.status_label)
        layout.addLayout(header)

        # 饼图
        self.pie = PieChart()
        layout.addWidget(self.pie, alignment=Qt.AlignmentFlag.AlignCenter)

        # 容量
        self.size_label = QLabel("")
        self.size_label.setStyleSheet("color: #b0b0b0; font-size: 12px;")
        self.size_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.addWidget(self.size_label)

        # 进度条
        self.progress_bar = QProgressBar()
        self.progress_bar.setVisible(False)
        self.progress_bar.setStyleSheet("""
            QProgressBar {
                height: 14px;
                border-radius: 7px;
                background: #3c3c3c;
                text-align: center;
                color: #ffffff;
            }
            QProgressBar::chunk {
                background: #FF9800;
                border-radius: 7px;
            }
        """)
        layout.addWidget(self.progress_bar)

        # 复制计数
        self.copy_count_label = QLabel("")
        self.copy_count_label.setStyleSheet("color: #9e9e9e; font-size: 11px;")
        self.copy_count_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.copy_count_label.setVisible(False)
        layout.addWidget(self.copy_count_label)

        # 当前复制文件名
        self.current_file_label = QLabel("")
        self.current_file_label.setStyleSheet("color: #808080; font-size: 10px;")
        self.current_file_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.current_file_label.setVisible(False)
        self.current_file_label.setWordWrap(True)
        layout.addWidget(self.current_file_label)

        # 按钮行
        btn_layout = QHBoxLayout()
        self.format_btn = QPushButton("格式化")
        self.format_btn.setVisible(False)
        self.format_btn.setStyleSheet("QPushButton { background: #F44336; color: white; border: none; border-radius: 4px; padding: 4px 8px; font-size: 12px; } QPushButton:hover { background: #e53935; }")
        self.format_btn.clicked.connect(lambda: self.format_clicked.emit(self._drive_letter))

        self.eject_btn = QPushButton("弹出")
        self.eject_btn.setVisible(False)
        self.eject_btn.setStyleSheet("QPushButton { background: #616161; color: white; border: none; border-radius: 4px; padding: 4px 8px; font-size: 12px; } QPushButton:hover { background: #757575; }")
        self.eject_btn.clicked.connect(lambda: self.eject_clicked.emit(self._drive_letter))

        self.cancel_btn = QPushButton("取消")
        self.cancel_btn.setVisible(False)
        self.cancel_btn.setStyleSheet("QPushButton { background: #757575; color: white; border: none; border-radius: 4px; padding: 4px 8px; font-size: 12px; } QPushButton:hover { background: #9e9e9e; }")
        self.cancel_btn.clicked.connect(lambda: self.cancel_clicked.emit(self._drive_letter))

        btn_layout.addWidget(self.format_btn)
        btn_layout.addWidget(self.eject_btn)
        btn_layout.addWidget(self.cancel_btn)
        layout.addLayout(btn_layout)

    def set_drive(self, drive_letter: str, label: str = "", total: int = 0, used: int = 0):
        self._drive_letter = drive_letter
        self.drive_label.setText(f"{drive_letter} {label}")
        self.size_label.setText(self._format_size(used, total))
        self.pie.set_percent((used / total * 100) if total > 0 else 0)
        self.set_status(self.STATUS_IDLE)

    def set_status(self, status: str):
        self._status = status
        self.progress_bar.setVisible(False)
        self.cancel_btn.setVisible(False)
        self.format_btn.setVisible(False)
        self.eject_btn.setVisible(False)
        self.copy_count_label.setVisible(False)
        self.current_file_label.setVisible(False)

        if status == self.STATUS_IDLE:
            self.status_label.setText("空闲")
            self.status_label.setStyleSheet("color: #9e9e9e; font-size: 11px;")
        elif status == self.STATUS_COPYING:
            self.status_label.setText("复制中...")
            self.status_label.setStyleSheet("color: #FF9800; font-size: 11px;")
            self.progress_bar.setVisible(True)
            self.cancel_btn.setVisible(True)
            self.copy_count_label.setVisible(True)
        elif status == self.STATUS_DONE:
            self.status_label.setText("待操作")
            self.status_label.setStyleSheet("color: #4CAF50; font-size: 11px;")
            self.format_btn.setVisible(True)
            self.eject_btn.setVisible(True)
        elif status == self.STATUS_FORMATTING:
            self.status_label.setText("格式化中...")
            self.status_label.setStyleSheet("color: #F44336; font-size: 11px;")

    def set_current_file(self, filename: str):
        self.current_file_label.setText(filename)
        self.current_file_label.setToolTip(filename)
        self.current_file_label.setVisible(True)

    def update_copy_progress(self, current: int, total: int, file_progress: float):
        self.progress_bar.setMaximum(total)
        self.progress_bar.setValue(current)
        self.copy_count_label.setText(f"{current}/{total} ({file_progress:.0f}%)")
        self.copy_count_label.setVisible(True)
        self.current_file_label.setVisible(True)

    def clear(self):
        self._drive_letter = ""
        self.drive_label.setText("空闲")
        self.status_label.setText("")
        self.size_label.setText("")
        self.pie.set_percent(0)
        self.progress_bar.setVisible(False)
        self.progress_bar.setValue(0)
        self.copy_count_label.setVisible(False)
        self.current_file_label.setVisible(False)
        self.format_btn.setVisible(False)
        self.eject_btn.setVisible(False)
        self.cancel_btn.setVisible(False)

    @staticmethod
    def _format_size(used: int, total: int) -> str:
        def fmt(b: int) -> str:
            if b >= 1024**3: return f"{b/1024**3:.1f}GB"
            if b >= 1024**2: return f"{b/1024**2:.1f}MB"
            if b >= 1024: return f"{b/1024:.1f}KB"
            return f"{b}B"
        return f"{fmt(used)} / {fmt(total)}"
