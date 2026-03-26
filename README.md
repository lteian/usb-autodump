# U盘自动转储工具

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Version](https://img.shields.io/github/v/release/lteian/usb-autodump?include_prereleases&label=version)](https://github.com/lteian/usb-autodump/releases)

USB 自动检测、视频文件转储、FTP 上传的 Windows/Linux 桌面工具。

## 功能特性

- 🔌 **多盘并发** - 同时插最多 8 个 U 盘，各自独立转储
- 📁 **视频转储** - 自动扫描 mp4/avi/mkv/mov 等视频文件，保留目录结构
- 📊 **容量显示** - 饼图 + 数字显示 U 盘已用/总容量
- ☁️ **FTP 上传** - 队列管理、断点续传、失败自动重试（≤3次）
- 🔐 **密码加密** - FTP 密码用 AES-256 加密存储
- 🗑️ **自动清理** - 上传完成后自动删除本地文件
- ⚙️ **U盘专属路径** - 每个盘符可配置不同的本地存放路径
- 💿 **格式化/弹出** - 复制完成后可一键格式化或弹出 U 盘

---

## 首次使用

### 1. 设置加密密码

打开 **设置 ⚙** → 在顶部找到 **🔐 加密密码** 输入框设置密码。

此密码用于加密你的 FTP 密码，**请务必牢记**。

> ⚠️ 遗忘加密密码将导致无法解密已保存的 FTP 密码。

### 2. 配置 FTP

设置好加密密码后，填写 FTP 服务器地址、端口、用户名、密码、子路径。

### 3. 设置本地路径（可选）

- 默认路径：`D:/U盘转储`
- 可为每个 U 盘盘符配置不同的本地存放路径

---

## 忘记密码怎么办？

打开 **设置 ⚙** → 点击底部 **🔑 忘记密码？重置所有配置**

此操作会**清空所有配置**（加密密码、FTP 密码、文件记录），请谨慎操作。

---

## 安装

### Windows（EXE）
下载最新 Release 中的 `.exe` 文件，双击运行。

👉 https://github.com/lteian/usb-autodump/releases

### Linux（DEB）- 统信 UOS / 麒麟 Kylin
```bash
sudo dpkg -i usb-autodump_1.0.0_amd64.deb
# 运行
usb-autodump
```

## 从源码运行

```bash
pip install PyQt6 pywin32 cryptography
python main.py
```

## 打包

### Windows EXE
```bash
pip install pyinstaller
pyinstaller --onefile --windowed main.py
```

### Linux DEB（统信/麒麟）
```bash
pip install pyinstaller
pyinstaller --onefile --name usb-autodump main.py
fpm -s dir -t deb -n usb-autodump -v 1.0.0 \
  -p dist/usb-autodump_1.0.0_amd64.deb \
  -f dist/deb_root
```

## 配置存放位置

| 平台 | 配置文件 | SQLite 数据库 |
|------|---------|--------------|
| Windows | `%USERPROFILE%\.config\usb_autodump\config.json` | `%USERPROFILE%\.local\share\usb_autodump\file_records.db` |
| Linux | `~/.config/usb_autodump/config.json` | `~/.local/share/usb_autodump/file_records.db` |

日志文件：`~/.local/logs/usb_autodump/`
