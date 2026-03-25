# ui/main_window.py
from PyQt6.QtWidgets import (QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
                             QLabel, QScrollArea, QStatusBar, QMessageBox,
                             QGridLayout, QFrame, QPushButton)
from PyQt6.QtCore import Qt, QTimer
from PyQt6.QtGui import QFont
from ui.usb_card import USBCard
from ui.log_panel import LogPanel
from ui.settings_dialog import SettingsDialog
from core.usb_monitor import USBMonitor, USBDevice
from core.copy_engine import CopyEngine
from core.ftp_uploader import FTPUploader
from core.disk_tool import format_drive, eject_drive
from core.file_record import get_pending_count_and_size, get_uploaded_count_and_size
from utils.logger import get_logger
from utils.config import is_password_set
from ui.settings_dialog import require_password_setup

logger = get_logger()
MAX_USB_SLOTS = 8


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        # 首次运行强制设置加密密码
        if not is_password_set():
            logger.info("首次运行，强制设置加密密码...")
            if not require_password_setup(self):
                logger.warning("用户未设置加密密码，直接退出")
                return
            logger.info("加密密码已设置")

        self._usb_monitor = USBMonitor()
        self._copy_engine = CopyEngine()
        self._ftp_uploader = FTPUploader()
        self._usb_cards: dict[str, USBCard] = {}
        self._cards: list[USBCard] = []
        self._setup_ui()
        self._setup_signals()
        self._start_services()
        self._status_timer = QTimer()
        self._status_timer.timeout.connect(self._update_status_bar)
        self._status_timer.start(2000)

    def _setup_ui(self):
        self.setWindowTitle("🚀 U盘自动转储工具")
        self.setMinimumSize(900, 600)
        self.setStyleSheet("QMainWindow { background: #1e1e1e; } QLabel { color: #e0e0e0; }")

        central = QWidget()
        self.setCentralWidget(central)
        layout = QVBoxLayout(central)

        # 顶部栏
        header = QFrame()
        header.setStyleSheet("background: #252525; border-bottom: 1px solid #3c3c3c; padding: 8px;")
        hlayout = QHBoxLayout(header)
        title = QLabel("🚀 U盘自动转储工具")
        title.setFont(QFont("Arial", 14, QFont.Weight.Bold))
        hlayout.addWidget(title)
        hlayout.addStretch()

        self.settings_btn = QPushButton("⚙ 设置")
        self.settings_btn.setStyleSheet("background: #424242; color: white; border: none; border-radius: 4px; padding: 6px 12px;")
        self.settings_btn.clicked.connect(self._open_settings)
        hlayout.addWidget(self.settings_btn)

        self.ftp_status_label = QLabel("FTP: ●未连接")
        self.ftp_status_label.setStyleSheet("color: #9e9e9e; font-size: 12px; padding: 0 8px;")
        hlayout.addWidget(self.ftp_status_label)
        layout.addWidget(header)

        # USB 卡片区
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.Shape.NoFrame)
        scroll.setStyleSheet("background: transparent;")

        cards_widget = QWidget()
        cards_layout = QGridLayout(cards_widget)
        cards_layout.setSpacing(12)

        for i in range(MAX_USB_SLOTS):
            card = USBCard()
            self._cards.append(card)
            cards_layout.addWidget(card, i // 4, i % 4)

        scroll.setWidget(cards_widget)
        layout.addWidget(scroll, stretch=1)

        # Log 面板
        self.log_panel = LogPanel()
        layout.addWidget(self.log_panel)

        # 状态栏
        self.status_bar = QStatusBar()
        self.setStatusBar(self.status_bar)
        self._update_status_bar()

    def _setup_signals(self):
        self._usb_monitor.add_callback(self._on_usb_event)
        self._copy_engine.add_progress_callback(self._on_copy_progress)
        self._copy_engine.add_done_callback(self._on_copy_done)
        self._ftp_uploader.add_callback(self._on_upload_done)

        for card in self._cards:
            card.format_clicked.connect(self._on_format_requested)
            card.eject_clicked.connect(self._on_eject_requested)
            card.cancel_clicked.connect(self._on_cancel_requested)

    def _start_services(self):
        self._usb_monitor.start()
        self._ftp_uploader.start()
        for dev in self._usb_monitor.get_current_devices():
            QTimer.singleShot(500, lambda d=dev: self._on_usb_event("insert", d))
        self.log_panel.append_info("服务已启动，等待 USB 设备...")

    def _on_usb_event(self, event: str, device: USBDevice):
        drive = device.drive_letter
        if event == "insert":
            self.log_panel.append_info(
                f"{drive} 插入，容量 {self._fmt_size(device.total_size)}，"
                f"已用 {device.used_space / device.total_size * 100:.0f}%" if device.total_size else ""
            )
            self._allocate_card(drive, device)
            self._start_copy_if_needed(drive)
        elif event == "remove":
            self.log_panel.append_info(f"{drive} 已移除")
            self._release_card(drive)

    def _allocate_card(self, drive_letter: str, device: USBDevice):
        for card in self._cards:
            if card.drive_label.text() in ("空闲", ""):
                card.set_drive(drive_letter, device.label, device.total_size, device.used_space)
                self._usb_cards[drive_letter] = card
                break

    def _release_card(self, drive_letter: str):
        card = self._usb_cards.pop(drive_letter, None)
        if card:
            card.clear()
        if self._copy_engine.is_copying(drive_letter):
            self._copy_engine.cancel_copy(drive_letter)

    def _start_copy_if_needed(self, drive_letter: str):
        card = self._usb_cards.get(drive_letter)
        if not card:
            return
        card.set_status(USBCard.STATUS_COPYING)
        self.log_panel.append_info("开始扫描视频文件...")
        self._copy_engine.start_copy(drive_letter)

    def _on_copy_progress(self, drive_letter: str, idx: int, total: int, task):
        card = self._usb_cards.get(drive_letter)
        if card:
            card.update_copy_progress(idx, total, task.progress)
            if task.status == "copied":
                self.log_panel.append_info(f"复制完成: {task.src_path}")

    def _on_copy_done(self, drive_letter: str, tasks):
        card = self._usb_cards.get(drive_letter)
        if not card:
            return
        copied = sum(1 for t in tasks if t.status == "copied")
        errors = sum(1 for t in tasks if t.status == "error")
        self.log_panel.append_info(f"{drive_letter} 复制完成: {copied}个成功，{errors}个失败")
        card.set_status(USBCard.STATUS_DONE)
        self._update_status_bar()

    def _on_upload_done(self, task):
        if task.status == "uploaded":
            self.log_panel.append_info(f"上传成功: {task.local_path}")
        elif task.status == "error":
            self.log_panel.append_error(f"上传失败: {task.local_path} - {task.error_msg}")
        self._update_status_bar()

    def _on_format_requested(self, drive_letter: str):
        card = self._usb_cards.get(drive_letter)
        if not card:
            return
        reply = QMessageBox.question(self, "确认格式化",
                                    f"确定要格式化 {drive_letter} 吗？\n所有数据将被清除！",
                                    QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No)
        if reply != QMessageBox.StandardButton.Yes:
            return
        card.set_status(USBCard.STATUS_FORMATTING)
        self.log_panel.append_warning(f"开始格式化 {drive_letter} ...")
        ok = format_drive(drive_letter)
        self.log_panel.append_info(f"格式化{'成功' if ok else '失败'}: {drive_letter}")
        if card:
            card.set_status(USBCard.STATUS_DONE)

    def _on_eject_requested(self, drive_letter: str):
        self.log_panel.append_info(f"弹出 {drive_letter} ...")
        ok = eject_drive(drive_letter)
        self.log_panel.append_info(f"{'已弹出' if ok else '弹出失败'}: {drive_letter}")

    def _on_cancel_requested(self, drive_letter: str):
        self._copy_engine.cancel_copy(drive_letter)
        self.log_panel.append_warning(f"已取消复制: {drive_letter}")
        card = self._usb_cards.get(drive_letter)
        if card:
            card.set_status(USBCard.STATUS_IDLE)

    def _open_settings(self):
        dlg = SettingsDialog(self)
        dlg.exec()

    def _update_status_bar(self):
        pn, ps = get_pending_count_and_size()
        un, us = get_uploaded_count_and_size()
        queue = self._ftp_uploader.get_queue_size()
        connected = self._ftp_uploader.is_connected()

        self.ftp_status_label.setText(f"FTP: {'●已连接' if connected else '○未连接'}")
        self.ftp_status_label.setStyleSheet(
            "color: #4CAF50; font-size: 12px; padding: 0 8px;" if connected
            else "color: #9e9e9e; font-size: 12px; padding: 0 8px;"
        )
        self.status_bar.showMessage(
            f"  待上传: {pn}个 ({self._fmt_size(ps)})  |  "
            f"已上传: {un}个 ({self._fmt_size(us)})  |  队列: {queue}"
        )

    @staticmethod
    def _fmt_size(b: int) -> str:
        if b >= 1024**3: return f"{b/1024**3:.1f}GB"
        if b >= 1024**2: return f"{b/1024**2:.1f}MB"
        if b >= 1024: return f"{b/1024:.1f}KB"
        return f"{b}B"

    def closeEvent(self, event):
        if hasattr(self, '_usb_monitor'):
            self._usb_monitor.stop()
        if hasattr(self, '_ftp_uploader'):
            self._ftp_uploader.stop()
        logger.info("应用退出")
        event.accept()
