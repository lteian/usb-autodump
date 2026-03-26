# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.1.0] - 2026-03-26

### Added
- **多进程架构** - 每个 U 盘的复制任务运行在独立进程中，互不干扰
- **独立 FTP 进程** - FTP 上传运行在独立子进程，不影响复制速度
- **ProcessManager** - 统一管理所有子进程的生命周期

### Fixed
- **文件复制后看不到** - `_copy_with_progress` 添加 `fsync()` 确保数据落盘，复制期间每 8MB 做一次中间 fsync，关闭前再做最终 fsync
- **第二次启动出现两个 H 盘** - 重启 app 时预填充 `_last_drives`，避免 poll loop 把已插入的 USB 当新设备处理两次
- **软件闪退** - USB 事件回调、复制进度回调、复制完成回调全部加 `try/except` 保护

### Changed
- `CopyEngine` 改为子进程代理，实际复制工作在独立 Process 中执行
- `FTPUploader` 改为子进程代理，上传工作在独立 Process 中执行
- `MainWindow` 通过 `Queue` 轮询子进程事件刷新 UI，不再直接持有复制线程

## [1.0.1] - 2026-03-25

### Fixed
- 使用 `python3` 和 `--break-system-packages` 安装 pip 依赖

## [1.0.0] - 2026-03-24

### Added
- USB 自动检测插入/拔出
- 视频文件自动扫描（mp4/avi/mkv/mov/wmv/flv/webm/m4v/mpg/mpeg）
- 复制过程进度显示
- FTP 队列上传、断点续传、失败自动重试（≤3次）
- FTP 密码 AES-256 加密存储
- 上传完成后自动删除本地文件
- 每个 U 盘可配置独立存放路径
- 一键格式化和弹出 U 盘
- 支持 Windows EXE 和 Linux DEB 打包
