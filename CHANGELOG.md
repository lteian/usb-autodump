# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.1.0] - 2026-03-26

### Fixed
- **文件复制后看不到** - `_copy_with_progress` 添加 `fsync()` 确保数据落盘，复制期间每 8MB 做一次中间 fsync，关闭前再做最终 fsync
- **第二次启动出现两个 H 盘** - 重启 app 时预填充 `_last_drives`，避免 poll loop 把已插入的 USB 当新设备处理两次
- **软件闪退** - USB 事件回调、复制进度回调、复制完成回调全部加 `try/except` 保护

### Added
- MIT LICENSE 开源协议

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
- 启动密码验证
- 待上传文件列表（盘符/文件名/大小/状态）
- 复制中显示当前文件名
