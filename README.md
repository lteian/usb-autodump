# USB 自动转储工具

USB 自动转储工具（usb-autodump）是一款面向 Windows / Linux 的 U盘视频文件自动转储与上传工具。采用 C++17 + Qt5 开发，支持 USB 实时检测、视频文件自动拷贝、FTP 上传、密码加密存储、格式化 / 弹出 U 盘等功能。

---

## 主要功能

### USB 检测与显示
- 实时检测 USB 插拔，最多 8 个设备同时显示
- 卡片显示：盘符、卷标、总容量、已用空间
- 状态指示：空闲 / 复制中 / 待操作 / 格式化中
- 容量横条（绿色表示已用空间比例）

### 文件转储
- 自动扫描视频文件（mp4 / avi / mkv / mov / wmv / flv / webm / m4v / mpg / mpeg）
- 保留原目录结构，复制到本地目标路径
- 每 U 盘独立线程并行处理，互不阻塞
- 进度显示：总进度条 + 当前文件进度

### FTP 上传
- 上传队列管理，支持 TLS 加密连接
- 失败自动重试（最多 3 次，可配置）
- 上传完成后自动删除本地文件（可配置）
- 路径编码可选（UTF-8 / GBK），兼容群晖等服务器

### 密码加密
- AES-256-CBC 加密 FTP 密码
- PBKDF2(SHA256, 100000 次) 派生密钥
- 遗忘密码可一键清空所有配置

### 格式化 / 弹出
- Windows：PowerShell `Format-Volume` 格式化
- Linux：mkfs.vfat 格式化 + umount 弹出
- 格式化成功后自动将 license.txt 复制到 U 盘根目录

---

## 构建

### Windows EXE

```bash
# 安装 Qt5（MSVC2019/2022）和 CMake
mkdir build && cd build
cmake .. -G "Visual Studio 16 2019"
cmake --build . --config Release
windeployqt ./dist/*/usb-autodump.exe
```

### Linux DEB

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
cpack -G DEB
```

---

## 配置

首次运行会在 exe 同目录生成 `config.json`：

```json
{
  "local_path": "D:/U盘转储",
  "ftp": {
    "host": "",
    "port": 21,
    "username": "",
    "password": "",
    "sub_path": "/",
    "use_tls": false,
    "max_retry": 3
  },
  "video_extensions": [".mp4",".avi",".mkv",".mov",".wmv",".flv",".webm",".m4v",".mpg",".mpeg"],
  "auto_delete_local": true,
  "auto_format_after_copy": false,
  "usb_paths": {}
}
```

---

## 项目结构

```
usb-autodump/
├── src/
│   ├── main.cpp              # 程序入口
│   ├── mainwindow.cpp/h      # 主窗口
│   ├── usb_card.cpp/h        # U盘设备卡片
│   ├── usb_monitor.cpp/h     # USB 热插拔监控
│   ├── dump_process.cpp/h    # 文件转储引擎
│   ├── ftp_pool.cpp/h        # FTP 连接池
│   ├── ftp_process.cpp/h     # FTP 上传处理
│   ├── disk_tool.cpp/h       # 磁盘格式化/弹出
│   ├── crypto.cpp/h          # AES-256 加密
│   ├── config.cpp/h          # 配置读写
│   ├── settings_dialog.cpp/h # 设置界面
│   ├── password_dialog.cpp/h # 密码设置对话框
│   ├── log_panel.cpp/h       # 日志面板
│   └── file_record.cpp/h     # 文件记录
├── linux/
│   └── usb-autodump.desktop  # Linux 桌面快捷方式
├── CMakeLists.txt
├── config.json
├── license.txt
└── .github/workflows/
    ├── build.yml             # Windows EXE 自动构建
    └── build-deb.yml         # Linux DEB 自动构建
```

---

## 依赖

| 库 | 版本 | 用途 |
|---|---|---|
| Qt5 Core/Widgets/Network/Sql | ≥ 5.15 | GUI + 网络 |
| CMake | ≥ 3.16 | 构建系统 |
| C++17 | — | 语言标准 |

优先使用 Qt 内置功能，无需额外加密库。

---

## 平台支持

| 平台 | USB 检测 | 格式化 |
|---|---|---|
| Windows 10/11 | WMI + diskpart | PowerShell Format-Volume |
| Linux (统信/麒麟) | lsblk / udev | mkfs.vfat / umount |

---

## 许可证

本项目仅供个人 / 内部使用。license.txt 位于 exe 同目录，格式化 U 盘时会自动复制到 U 盘根目录。
