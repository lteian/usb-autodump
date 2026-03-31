# USB 自动转储工具 - C++/Qt5 版本 SPEC

## 1. 概述

- **名称**: USB 自动转储工具 (usb-autodump)
- **语言**: C++17
- **GUI框架**: Qt5
- **目标平台**: Windows EXE / Linux DEB (统信/麒麟)
- **功能**: USB 自动检测、视频文件转储、FTP 上传、格式化/弹出 U 盘

---

## 2. 项目结构

```
usb_autodump_cpp/
├── SPEC.md
├── CMakeLists.txt
├── config.json
├── src/
│   ├── main.cpp
│   ├── mainwindow.h / mainwindow.cpp
│   ├── usd_card.h / usd_card.cpp
│   ├── log_panel.h / log_panel.cpp
│   ├── settings_dialog.h / settings_dialog.cpp
│   ├── usb_monitor.h / usb_monitor.cpp
│   ├── copy_engine.h / copy_engine.cpp
│   ├── ftp_uploader.h / ftp_uploader.cpp
│   ├── disk_tool.h / disk_tool.cpp
│   ├── file_record.h / file_record.cpp
│   ├── crypto.h / crypto.cpp
│   └── config.h / config.cpp
├── linux/
│   └── usb-autodump.desktop
└── .github/workflows/
    ├── build.yml       # Windows EXE
    └── build-deb.yml   # Linux DEB
```

---

## 3. 功能规格

### 3.1 USB 检测与显示
- 实时检测 USB 插拔，最多 8 个盘符
- 显示：盘符、卷标、总容量、已用空间
- 饼状图（QCustomPlot 或 QPainter 自绘）
- 状态：空闲 / 复制中 / 待操作 / 格式化中

### 3.2 文件转储
- 扫描视频文件（mp4/avi/mkv/mov/wmv/flv/webm/m4v/mpg/mpeg）
- 保留原目录结构，复制到本地
- 每 U 盘独立 QThread 多线程
- 进度：总进度条 + 当前文件进度

### 3.3 FTP 上传
- QtNetwork QNetworkAccessManager 或 raw socket FTP
- 上传队列管理，失败重试（≤3次）
- 上传完成后删除本地文件（根据配置）
- 连接状态显示

### 3.4 密码加密
- AES-256 CBC 加密（QCA 或 Botan 或直接用 Qt）
- 加密密码用户设置，存储在 config.json
- 遗忘密码 → 一键清空所有配置

### 3.5 设置界面
- 默认本地路径 + 每盘符专属路径
- FTP：地址/端口/用户名/密码/子路径/TLS
- 高级：自动格式化、自动删除本地、重试次数

### 3.6 格式化 / 弹出
- Windows: QProcess 调用 diskpart
- Linux: QProcess 调用 mkfs.vfat / umount

---

## 4. UI 布局

```
┌─────────────────────────────────────────────────────┐
│ 🚀 U盘自动转储工具         [⚙ 设置]  [FTP: ●]         │
├─────────────────────────────────────────────────────┤
│ [设备卡片区 - 4列GridLayout]                         │
│ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐               │
│ │ E:\  │ │ F:\  │ │      │ │      │               │
│ │[饼图]│ │[饼图]│ │(空闲)│ │(空闲)│               │
│ │12GB │ │ 8GB  │ │      │ │      │               │
│ │[格式化]│ [取消]│ │      │ │      │               │
│ │[弹出]│ │      │ │      │ │      │               │
│ └──────┘ └──────┘ └──────┘ └──────┘               │
├─────────────────────────────────────────────────────┤
│ [Log面板 - QTextEdit 只读]                         │
├─────────────────────────────────────────────────────┤
│ 待上传: 3个(1.2GB)  已上传: 12个(8.5GB)  [状态栏]  │
└─────────────────────────────────────────────────────┘
```

---

## 5. 配置格式 (config.json)

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

## 6. 加密方案

- 使用 Qt 的 QCryptographicHash 或集成 Botan/libsodium
- PBKDF2(SHA256, 100000次) 派生密钥
- AES-256-CBC 加密 FTP 密码
- 密钥 = PBKDF2(用户设置的密码, 固定盐)
- config.json 中存加密后密文（Base64 编码）

---

## 7. 构建要求

### Windows EXE
- Qt5 MSVC2019/2022 静态或动态链接
- Qt Creator 或 qmake / CMake
- windeployqt 打包
- GitHub Actions: Windows-latest + Qt5 安装

### Linux DEB
- CMake + Qt5
- fpm 打包
- .desktop 文件 + 应用图标

---

## 8. 平台适配

| 平台 | USB检测 | 格式化 |
|------|---------|--------|
| Windows | WMI via QProcess + wmic | diskpart |
| Linux (统信/麒麟) | QProcess + lsblk / udev | mkfs.vfat / umount |

---

## 9. 依赖（外部库）

- **Qt5 Core / Widgets / Network / Serial**（必须）
- **QuaZIP**（ZIP处理可选）
- **Botan** 或 **libsodium**（加密，如不用 Qt 内置）

优先使用 Qt 内置功能，减少外部依赖。

---

## 10. 变更日志 (Changelog)

### v2026-03-31

**FTP 编码支持**
- 添加 FTP 路径编码选项（UTF-8 / GBK），解决群晖等服务器中文路径乱码
- MKD（创建目录）和 STOR（上传文件）命令根据编码设置发送路径
- 设置路径：设置 → FTP设置 → 路径编码

**U盘卡片 UI 优化**
- 显示容量指示条（绿色横条表示已用空间比例）
- 状态显示优化：
  - 无视频文件 → 显示"无视频文件"（灰色）
  - 有文件转储完成 → 显示"转储完成"（绿色）
  - 复制中 → 显示"复制中"（蓝色）

**格式化功能修复**
- 改用 PowerShell `Format-Volume` 替代 `diskpart`，修复格式化失败问题
- 格式化成功后将 `license.txt`（位于 exe 同目录）复制到U盘根目录

**其他修复**
- 修复 mainwindow.cpp QLayout 警告（leftVl 不能 addWidget 自身）

### v2026-03-29

**初始版本**
- USB 自动检测与显示（最多4个卡片）
- 视频文件转储（mp4/avi/mkv/mov/wmv/flv/webm/m4v/mpg/mpeg）
- FTP 上传队列管理（支持 TLS）
- 密码加密存储（AES-256）
- 本地/远程日志系统（按天滚动，5MB限制）
- GitHub Actions 自动构建 Release
