# USB 自动转储工具 - SPEC.md

## 1. 项目概述

- **项目名称**: USB 自动转储工具 (usb-autodump)
- **项目类型**: Windows/Linux 桌面应用程序
- **核心功能**: USB U盘自动检测、视频文件转储、本地上传FTP、格式化U盘
- **目标用户**: 需要批量转储U盘视频内容的用户

## 2. 技术栈

- **语言**: Python 3.10+
- **GUI框架**: PyQt6
- **USB检测**: WMI (Windows) / pyudev (Linux)
- **数据库**: SQLite
- **打包**: PyInstaller (Windows EXE) / fpm (Linux DEB)

## 3. 功能列表

### 3.1 USB 检测与显示
- 实时检测 USB 插拔事件
- 支持同时检测多个U盘（最多8个）
- 显示U盘盘符、容量、已用空间
- 饼状图显示空间使用情况

### 3.2 文件转储
- 自动扫描U盘内视频文件（mp4/avi/mkv/mov/wmv/flv/webm/m4v/mpg/mpeg）
- 保留原目录结构复制到本地路径
- 多线程并发复制，互不干扰
- 实时显示复制进度

### 3.3 FTP 上传
- 维护上传队列
- 支持配置 FTP 服务器、子路径、TLS/SSL
- 断点续传，失败自动重试（≤3次）
- 上传完成后自动删除本地文件

### 3.4 U盘格式化
- 复制+上传完成后，按钮变为「格式化」和「弹出」
- 调用 Windows diskpart 执行格式化
- 格式化前确认

### 3.5 Log 面板
- 实时显示操作记录
- 显示：待上传数量/大小、已上传数量/大小
- 每个U盘独立状态指示

### 3.6 设置界面
- 本地存储路径配置
- FTP 服务器/端口/用户名/密码/子路径/TLS配置
- 复制完成后自动格式化（开关）
- 上传后自动删除本地文件（开关）
- 重试次数配置

## 4. UI 布局

```
┌────────────────────────────────────────────────────────────┐
│  🚀 U盘转储工具              [⚙ 设置]  [FTP状态指示]       │
├────────────────────────────────────────────────────────────┤
│  [USB设备卡片区域 - 横向滚动，多个U盘并行显示]              │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐                  │
│  │ E:\      │  │ F:\      │  │          │                  │
│  │ [饼图]   │  │ [饼图]   │  │  (空闲)  │                  │
│  │ 32GB/64GB│  │ 16GB/32GB│  │          │                  │
│  │ 状态指示  │  │ 复制进度 │  │          │                  │
│  │ [格式化] │  │ [取消]   │  │          │                  │
│  │ [弹出]   │  │          │  │          │                  │
│  └──────────┘  └──────────┘  └──────────┘                  │
├────────────────────────────────────────────────────────────┤
│  Log 面板                                                 │
│  [时间戳] 操作描述                                        │
├────────────────────────────────────────────────────────────┤
│  本地待上传: N个 (X.X GB)   已上传: N个 (X.X GB)  [清队列] │
└────────────────────────────────────────────────────────────┘
```

## 5. 数据设计

### 5.1 文件记录表 (file_records)

| 字段 | 类型 | 描述 |
|------|------|------|
| id | INTEGER | 主键 |
| usb_drive | TEXT | U盘盘符 |
| file_path | TEXT | 原始路径 |
| local_path | TEXT | 本地存放路径 |
| ftp_path | TEXT | FTP上传路径 |
| status | TEXT | pending/copying/uploaded/deleted/error |
| file_size | INTEGER | 文件大小(字节) |
| copied_at | DATETIME | 复制完成时间 |
| uploaded_at | DATETIME | 上传完成时间 |
| deleted_at | DATETIME | 删除时间 |
| error_msg | TEXT | 错误信息 |

## 6. 跨平台策略

| 平台 | USB检测 | 格式化 |
|------|---------|--------|
| Windows | WMI | diskpart |
| Linux (统信/麒麟) | pyudev + dbus | mkfs.vfat / dd |
| macOS | pyobjc + IOKit | diskutil |
| Android | 暂不支持（后续 Kotlin 重写）| - |

## 7. 配置文件 (config.json)

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
  "video_extensions": [".mp4", ".avi", ".mkv", ".mov", ".wmv", ".flv", ".webm", ".m4v", ".mpg", ".mpeg"],
  "auto_delete_local_after_upload": true,
  "auto_format_after_copy": false,
  "max_concurrent_uploads": 2
}
```

## 8. 打包计划

- **Windows**: PyInstaller --onefile --windowed main.py
- **Linux (DEB)**: fpm 打包
- **Android**: 后续 Kotlin 重写
