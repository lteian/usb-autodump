# USB Auto Dump - Synology NAS 安装指南

## 概述

本项目提供在 Synology NAS 上安装 USB Auto Dump 的完整方案。由于 Synology SPK 包需要在真实的 Synology 环境中使用 Synology Toolchain 构建，本指南提供两种安装方式。

---

## 安装方式

### 方式一：一键在线安装（推荐）

在 Synology NAS 的 SSH 终端中运行：

```bash
# 下载并运行安装脚本
curl -sSL https://raw.githubusercontent.com/lteian/usb-autodump/web-version/INSTALL.sh | bash
```

安装完成后访问：`http://<你的NAS-IP>:8765`

---

### 方式二：下载包后本地安装

#### Step 1: 下载安装包

**方法 A: 从 GitHub Actions 构建产物下载**
1. 访问 [Actions](https://github.com/lteian/usb-autodump/actions)
2. 选择 `Package for Synology` 或 `Build Synology Package` 工作流
3. 下载构建产物 `synology-package.tar.gz`

**方法 B: 从 Release 下载**
1. 访问 [Releases](https://github.com/lteian/usb-autodump/releases)
2. 下载 `*-synology-package.tar.gz`

#### Step 2: 上传到 NAS

```bash
# 使用 scp 上传
scp usb-autodump-*-synology-package.tar.gz user@nas:/volume1/tmp/

# 或使用 Synology File Station 上传
```

#### Step 3: 解压并安装

```bash
# SSH 登录到 NAS
ssh admin@<你的NAS-IP>

# 解压
cd /volume1/tmp
tar -xzvf usb-autodump-*-synology-package.tar.gz
cd usb-autodump-*

# 运行安装脚本
chmod +x INSTALL.sh
sudo ./INSTALL.sh
```

---

## 关于 SPK 包

### 为什么没有直接提供 .spk 文件？

Synology SPK 包的实际构建需要：
1. **Synology Toolchain** - 交叉编译工具链（不同架构有不同的 toolchain）
2. **真实的 NAS 固件环境** - 用于测试和验证
3. **Synology 签名证书** - 可选，但建议用于正式发布

GitHub Actions 的 Ubuntu/Windows 环境无法满足以上条件。

### 如何自己构建 .spk 文件

如果你需要生成正式的 SPK 安装包：

#### 方式 A: 在 NAS 本地构建

```bash
# 1. 在 NAS 上安装 Synology Toolchain (需要 DSM 7.x)
#    下载地址: https://www.synology.com/zh-cn/developer/beta#toolchain

# 2. 克隆源码
git clone https://github.com/lteian/usb-autodump.git
cd usb-autodump
git checkout web-version

# 3. 安装依赖
pip install pyudev psutil fastapi uvicorn httpx python-multipart

# 4. 运行构建
cd spk_scripts
chmod +x build_spk.sh
./build_spk.sh

# 输出的 .spk 文件在 spk_scripts/dist/ 目录
```

#### 方式 B: 使用 Docker 交叉编译

```dockerfile
# Dockerfile.spk-build
FROM debian:bullseye

# 安装 Synology toolchain (需要提前下载)
COPY synology-toolchain /opt/toolchain

ENV PATH="/opt/toolchain/bin:$PATH"
ENV CC=... # 根据目标架构设置

RUN apt-get update && apt-get install -y python3 python3-pip git

# 后续构建步骤...
```

---

## 安装后配置

### 首次配置

1. 访问 Web UI：`http://<NAS-IP>:8765`
2. 编辑配置文件：

```bash
nano /var/packages/usb-autodump/target/config/config.json
```

关键配置项：

```json
{
    "ftp": {
        "host": "你的FTP服务器",
        "port": 21,
        "username": "FTP用户名",
        "password": "FTP密码",
        "remote_path": "/上传目录"
    },
    "usb": {
        "auto_mount": true,
        "eject_after_copy": false
    },
    "general": {
        "log_level": "INFO",
        "port": 8765,
        "auto_start": true
    }
}
```

### 服务管理命令

```bash
# 启动服务
/var/packages/usb-autodump/target/scripts/startup.sh

# 停止服务
/var/packages/usb-autodump/target/scripts/stop.sh

# 重启服务
/var/packages/usb-autodump/target/scripts/stop.sh
/var/packages/usb-autodump/target/scripts/startup.sh

# 查看日志
tail -f /var/packages/usb-autodump/target/logs/usb-autodump.log
```

---

## 支持的架构

本包支持以下 Synology NAS 架构：

| 架构标识 | 说明 |
|---------|------|
| `x86_64` | Intel/AMD 64位 (大部分型号) |
| `broadwell` | Intel Broadwell (DS916+, DS716+, etc.) |
| `broadwellnk` | Intel Broadwell no-key |
| `v1000` | AMD V1000 (DS918+, etc.) |
| `geminilake` | Intel Gemini Lake (DS920+, DS720+, etc.) |

---

## 卸载

```bash
# SSH 到 NAS
ssh admin@<你的NAS-IP>

# 运行卸载脚本
sudo /var/packages/usb-autodump/target/scripts/uninstaller.sh

# 或手动删除
sudo rm -rf /var/packages/usb-autodump
```

---

## 故障排除

### 服务无法启动

```bash
# 检查 Python 是否可用
python3 --version

# 检查端口是否被占用
netstat -tlnp | grep 8765

# 查看详细错误日志
cat /var/packages/usb-autodump/target/var/usb-autodump.log
```

### USB 设备未检测

```bash
# 检查 USB 设备
lsusb

# 检查 pyudev
python3 -c "import pyudev; print('OK')"
```

### FTP 上传失败

```bash
# 测试 FTP 连接
ftp <你的FTP服务器>
# 手动输入用户名密码测试
```

---

## 工作流说明

### GitHub Actions 工作流

| 工作流 | 触发条件 | 说明 |
|--------|---------|------|
| `build.yml` | push 到 web-version / 手动触发 | 构建源码包 |
| `package.yml` | push 到 web-version / 手动触发 | 创建 Synology 安装包 |
| `release.yml` | 打 tag (v*) / 手动触发 | 创建 GitHub Release |

### 构建产物

每个工作流运行后会生成以下产物：

- `usb-autodump-*-source.tar.gz` - 源码包
- `usb-autodump-*-synology-package.tar.gz` - Synology 安装包

---

## 更多信息

- **项目主页**: https://github.com/lteian/usb-autodump
- **问题反馈**: https://github.com/lteian/usb-autodump/issues
- **Web 版本**: https://github.com/lteian/usb-autodump/tree/web-version

---

## 许可证

MIT License
