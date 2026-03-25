# U盘自动转储工具

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

## 首次使用

### 第一步：设置加密密码

**首次运行时，软件会强制弹出密码设置对话框。**

此密码用于加密你的 FTP 密码，请务必妥善保管。遗忘后将无法解密已保存的 FTP 密码。

### 第二步：配置 FTP（设置 ⚙ 按钮）

- 服务器地址 / 端口 / 用户名 / 密码
- 子路径（上传到 FTP 的哪个目录）
- 如需 TLS 加密，勾选对应选项

### 第三步：设置本地路径（可选）

- 默认路径：D:/U盘转储
- 可为每个 U 盘盘符配置专属路径

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

## 依赖

- Python 3.10+
- PyQt6
- pywin32（Windows USB 检测）
- cryptography（AES-256 加密）
- pyudev（Linux USB 检测）

## 从源码运行

```bash
pip install PyQt6 pywin32 cryptography
python main.py
```

## 打包

### Windows EXE
```bash
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

## 目录结构

```
~/.config/usb_autodump/config.json   # 配置文件（含加密后的FTP密码）
~/.local/share/usb_autodump/         # SQLite 数据库
~/.local/logs/usb_autodump/          # 日志文件
```
