# ui/settings_dialog.py
from PyQt6.QtWidgets import (QDialog, QVBoxLayout, QHBoxLayout, QLabel,
                             QLineEdit, QPushButton, QCheckBox, QSpinBox,
                             QGroupBox, QFormLayout, QFileDialog, QMessageBox,
                             QTableWidget, QTableWidgetItem, QHeaderView)
from PyQt6.QtCore import Qt
from utils.config import load_config, save_config
from utils.logger import get_logger

logger = get_logger()


class SettingsDialog(QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("⚙ 设置")
        self.setMinimumWidth(540)
        self._config = load_config()
        self._setup_ui()

    def _setup_ui(self):
        layout = QVBoxLayout(self)

        # ── 本地存储 ─────────────────────────────────────
        local_group = QGroupBox("本地存储")
        local_layout = QVBoxLayout()

        global_row = QHBoxLayout()
        global_row.addWidget(QLabel("默认路径:"))
        self.local_path_edit = QLineEdit(self._config.get("local_path", "D:/U盘转储"))
        self.local_path_edit.setReadOnly(True)
        self.browse_btn = QPushButton("浏览...")
        self.browse_btn.clicked.connect(self._browse_local_path)
        global_row.addWidget(self.local_path_edit)
        global_row.addWidget(self.browse_btn)
        local_layout.addLayout(global_row)

        local_layout.addWidget(QLabel("U盘专属路径（空则使用默认路径）:"))
        self.usb_paths_table = QTableWidget()
        self.usb_paths_table.setColumnCount(2)
        self.usb_paths_table.setHorizontalHeaderLabels(["盘符", "本地路径"])
        self.usb_paths_table.horizontalHeader().setSectionResizeMode(1, QHeaderView.ResizeMode.Stretch)
        self.usb_paths_table.setRowCount(4)
        self.usb_paths_table.setMinimumHeight(120)
        usb_paths = self._config.get("usb_paths", {})
        for i, (drive, path) in enumerate(usb_paths.items()):
            if i >= 4:
                break
            self.usb_paths_table.setItem(i, 0, QTableWidgetItem(drive))
            self.usb_paths_table.setItem(i, 1, QTableWidgetItem(path))
        local_layout.addWidget(self.usb_paths_table)
        local_group.setLayout(local_layout)
        layout.addWidget(local_group)

        # ── FTP 设置 ─────────────────────────────────────
        ftp_group = QGroupBox("FTP 设置（密码已加密存储）")
        ftp_layout = QFormLayout()

        self.ftp_host_edit = QLineEdit(self._config.get("ftp", {}).get("host", ""))
        self.ftp_port_spin = QSpinBox()
        self.ftp_port_spin.setRange(1, 65535)
        self.ftp_port_spin.setValue(self._config.get("ftp", {}).get("port", 21))
        self.ftp_user_edit = QLineEdit(self._config.get("ftp", {}).get("username", ""))
        self.ftp_pass_edit = QLineEdit()
        self.ftp_pass_edit.setEchoMode(QLineEdit.EchoMode.Password)
        self.ftp_pass_edit.setPlaceholderText("留空则不修改当前密码")
        self.ftp_subpath_edit = QLineEdit(self._config.get("ftp", {}).get("sub_path", "/"))
        self.ftp_tls_check = QCheckBox("使用 TLS/SSL (FTPS)")
        self.ftp_tls_check.setChecked(self._config.get("ftp", {}).get("use_tls", False))

        ftp_layout.addRow("服务器地址:", self.ftp_host_edit)
        ftp_layout.addRow("端口:", self.ftp_port_spin)
        ftp_layout.addRow("用户名:", self.ftp_user_edit)
        ftp_layout.addRow("密码:", self.ftp_pass_edit)
        ftp_layout.addRow("子路径:", self.ftp_subpath_edit)
        ftp_layout.addRow("", self.ftp_tls_check)
        ftp_group.setLayout(ftp_layout)
        layout.addWidget(ftp_group)

        # ── 高级选项 ─────────────────────────────────────
        adv_group = QGroupBox("高级选项")
        adv_layout = QVBoxLayout()
        self.auto_format_check = QCheckBox("复制完成后自动格式化U盘")
        self.auto_format_check.setChecked(self._config.get("auto_format_after_copy", False))
        self.auto_delete_check = QCheckBox("上传后自动删除本地文件")
        self.auto_delete_check.setChecked(self._config.get("auto_delete_local_after_upload", True))
        self.retry_spin = QSpinBox()
        self.retry_spin.setRange(1, 10)
        self.retry_spin.setValue(self._config.get("ftp", {}).get("max_retry", 3))

        retry_row = QHBoxLayout()
        retry_row.addWidget(QLabel("重试次数:"))
        retry_row.addWidget(self.retry_spin)
        retry_row.addStretch()

        adv_layout.addWidget(self.auto_format_check)
        adv_layout.addWidget(self.auto_delete_check)
        adv_layout.addLayout(retry_row)
        adv_group.setLayout(adv_layout)
        layout.addWidget(adv_group)

        # ── 按钮 ─────────────────────────────────────────
        btn_layout = QHBoxLayout()
        btn_layout.addStretch()
        self.cancel_btn = QPushButton("取消")
        self.cancel_btn.clicked.connect(self.reject)
        self.save_btn = QPushButton("保存")
        self.save_btn.setDefault(True)
        self.save_btn.clicked.connect(self._save)
        btn_layout.addWidget(self.cancel_btn)
        btn_layout.addWidget(self.save_btn)
        layout.addLayout(btn_layout)

    def _browse_local_path(self):
        path = QFileDialog.getExistingDirectory(self, "选择默认本地存储路径",
                                                self.local_path_edit.text())
        if path:
            self.local_path_edit.setText(path)

    def _save(self):
        # 收集U盘专属路径
        usb_paths = {}
        for row in range(self.usb_paths_table.rowCount()):
            d_item = self.usb_paths_table.item(row, 0)
            p_item = self.usb_paths_table.item(row, 1)
            if d_item and p_item and d_item.text().strip():
                drive = d_item.text().strip().rstrip("\\/")
                if not drive.endswith(":"):
                    drive += ":"
                usb_paths[drive] = p_item.text().strip()

        # 密码：留空则保留原密码
        new_pass = self.ftp_pass_edit.text().strip()
        old_pass = self._config.get("ftp", {}).get("password", "")
        password = new_pass if new_pass else old_pass

        self._config["local_path"] = self.local_path_edit.text().strip()
        self._config["usb_paths"] = usb_paths
        self._config["ftp"] = {
            "host": self.ftp_host_edit.text().strip(),
            "port": self.ftp_port_spin.value(),
            "username": self.ftp_user_edit.text().strip(),
            "password": password,
            "sub_path": self.ftp_subpath_edit.text().strip().rstrip("/"),
            "use_tls": self.ftp_tls_check.isChecked(),
            "max_retry": self.retry_spin.value()
        }
        self._config["auto_format_after_copy"] = self.auto_format_check.isChecked()
        self._config["auto_delete_local_after_upload"] = self.auto_delete_check.isChecked()

        try:
            save_config(self._config)
            logger.info("配置已保存（密码已加密）")
            QMessageBox.information(self, "保存成功",
                                    "配置已保存，密码已用 AES-256 加密存储。\n重启后生效。")
            self.accept()
        except Exception as e:
            QMessageBox.critical(self, "保存失败", f"保存配置失败:\n{e}")
