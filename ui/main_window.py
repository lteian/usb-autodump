# ui/main_window.py
"""
重构后：MainWindow 通过 ProcessManager 管理子进程，
通过队列事件刷新 UI，不再直接调用 copy_engine 的线程方法。
"""
import multiprocessing as mp
from PyQt6.QtWidgets import (QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
                             QLabel, QScrollArea, QStatusBar, QMessageBox,
                             QGridLayout, QFrame, QPushButton)
from PyQt6.QtCore import Qt, QTimer
from PyQt6.QtGui import QFont
from ui.usb_card import USBCard
from ui.log_panel import LogPanel
from ui.settings_dialog import SettingsDialog
from core.usb_monitor import USBMonitor, USBDevice
from core.disk_tool import format_drive, eject_drive
from core.file_record import get_pending_count_and_size, get_uploaded_count_and_size, get_records_by_status
from core.process_manager import ProcessManager
from utils.logger import get_logger
from utils.config import is_password_set, get_ftp_config, load_config
from pathlib import Path

logger = get_logger()
MAX_USB_SLOTS = 8


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self._usb_monitor = USBMonitor()
        self._usb_cards: dict[str, USBCard] = {}
        self._cards: list[USBCard] = []
        self._event_queue = mp.Queue()
        self._process_manager = ProcessManager(self._event_queue)
        self._copy_drive_map: dict[str, str] = {}  # drive_letter -> drive_letter (for completed copies)
        self._setup_ui()
        self._setup_signals()
        self._start_services()
        self._status_timer = QTimer()
        self._status_timer.timeout.connect(self._update_status_bar)
        self._status_timer.start(2000)

        # UI 刷新定时器 - 从队列获取子进程事件
        self._poll_timer = QTimer()
        self._poll_timer.timeout.connect(self._poll_child_events)
        self._poll_timer.start(100)

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

        for card in self._cards:
            card.format_clicked.connect(self._on_format_requested)
            card.eject_clicked.connect(self._on_eject_requested)
            card.cancel_clicked.connect(self._on_cancel_requested)

    def _start_services(self):
        # 预填充当前已插入的 USB，避免 poll loop 把它们当新设备处理两次
        for dev in self._usb_monitor.get_current_devices():
            self._usb_monitor._last_drives[dev.drive_letter] = dev

        # 启动 FTP 子进程
        self._process_manager.start_ftp_worker()

        self._usb_monitor.start()

        # 触发已插入设备的复制
        for dev in self._usb_monitor.get_current_devices():
            QTimer.singleShot(500, lambda d=dev: self._on_usb_event("insert", d))

        self.log_panel.append_info("服务已启动，等待 USB 设备...")

    def _poll_child_events(self):
        """轮询子进程事件队列，刷新 UI"""
        try:
            while True:
                event = self._event_queue.get_nowait()
                self._handle_child_event(event)
        except:
            pass

    def _handle_child_event(self, event: dict):
        etype = event.get("type", "")

        if etype == "copy_scan_start":
            drive_letter = event.get("drive_letter", "")
            card = self._usb_cards.get(drive_letter)
            if card:
                card.set_status(USBCard.STATUS_COPYING)
                self.log_panel.append_info("开始扫描视频文件...")

        elif etype == "copy_scan_done":
            drive_letter = event.get("drive_letter", "")
            total = event.get("total", 0)
            self.log_panel.append_info(f"扫描完成，共 {total} 个视频文件")

        elif etype == "copy_progress":
            drive_letter = event.get("drive_letter", "")
            idx = event.get("idx", 0)
            total = event.get("total", 0)
            progress = event.get("progress", 0)
            status = event.get("status", "copying")
            card = self._usb_cards.get(drive_letter)
            if card:
                card.update_copy_progress(idx, total, progress)
                if status == "copied":
                    src_path = event.get("src_path", "")
                    self.log_panel.append_info(f"复制完成: {src_path}")

        elif etype == "copy_done":
            drive_letter = event.get("drive_letter", "")
            total = event.get("total", 0)
            copied = event.get("copied", 0)
            errors = event.get("errors", 0)
            card = self._usb_cards.get(drive_letter)
            if card:
                card.set_status(USBCard.STATUS_DONE)
                self.log_panel.append_info(f"{drive_letter} 复制完成: {copied}个成功，{errors}个失败")
                # 通知 FTP 上传
                self._enqueue_ftp_uploads(drive_letter)
            self._update_status_bar()

        elif etype == "copy_error":
            drive_letter = event.get("drive_letter", "")
            error_msg = event.get("error_msg", "未知错误")
            self.log_panel.append_error(f"{drive_letter} 复制错误: {error_msg}")

        elif etype == "ftp_status":
            connected = event.get("connected", False)
            self.ftp_status_label.setText(f"FTP: {'●已连接' if connected else '○未连接'}")
            self.ftp_status_label.setStyleSheet(
                "color: #4CAF50; font-size: 12px; padding: 0 8px;" if connected
                else "color: #9e9e9e; font-size: 12px; padding: 0 8px;"
            )

        elif etype == "ftp_success":
            local_path = event.get("local_path", "")
            self.log_panel.append_info(f"上传成功: {local_path}")
            self._update_status_bar()

        elif etype == "ftp_error":
            local_path = event.get("local_path", "")
            error_msg = event.get("error_msg", "未知错误")
            self.log_panel.append_error(f"上传失败: {local_path} - {error_msg}")
            self._update_status_bar()

        elif etype == "ftp_deleted":
            local_path = event.get("local_path", "")
            self.log_panel.append_info(f"本地文件已删除: {local_path}")

    def _enqueue_ftp_uploads(self, drive_letter: str):
        """将复制完成的文件加入 FTP 上传队列"""
        cfg = get_ftp_config()
        sub_path = cfg.get("sub_path", "/")

        records = get_records_by_status("copied")
        for rec in records:
            local_path = rec.get("local_path", "")
            if not local_path:
                continue
            # 只处理该盘符对应的文件
            if not local_path.startswith(drive_letter.rstrip("\\/")):
                continue
            ftp_sub = f"{sub_path.rstrip('/')}/{Path(local_path).name}"
            self._process_manager.send_ftp_task(
                rec["id"],
                local_path,
                ftp_sub,
                rec.get("file_size", 0) or 0
            )
        self._update_status_bar()

    def _on_usb_event(self, event: str, device: USBDevice):
        try:
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
        except Exception as e:
            logger.error(f"_on_usb_event 异常: {e}")

    def _allocate_card(self, drive_letter: str, device: USBDevice):
        for card in self._cards:
            if card.drive_label.text() in ("空闲", ""):
                card.set_drive(drive_letter, device.label, device.total_size, device.used_space)
                self._usb_cards[drive_letter] = card
                break

    def _release_card(self, drive_letter: str):
        # 停止复制进程
        self._process_manager.stop_copy_worker(drive_letter)
        card = self._usb_cards.pop(drive_letter, None)
        if card:
            card.clear()

    def _start_copy_if_needed(self, drive_letter: str):
        # 检查是否已有复制进程在运行
        if self._process_manager.is_copy_worker_alive(drive_letter):
            return
        # 启动复制
        self._process_manager.start_copy_worker(drive_letter, self._get_dest_dir(drive_letter))

    def _get_dest_dir(self, drive_letter: str) -> str:
        from utils.config import get_local_path
        local_base = Path(get_local_path(drive_letter))
        return str(local_base / drive_letter.rstrip("\\/"))

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
        self._process_manager.stop_copy_worker(drive_letter)
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
        queue = self._process_manager.get_ftp_queue_size()

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
        self._poll_timer.stop()
        self._status_timer.stop()
        if hasattr(self, '_usb_monitor'):
            self._usb_monitor.stop()
        if hasattr(self, '_process_manager'):
            self._process_manager.stop_all()
        logger.info("应用退出")
        event.accept()
