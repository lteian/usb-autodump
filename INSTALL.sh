#!/bin/bash
#
# INSTALL.sh - USB Auto Dump Synology NAS 一键安装脚本
#
# 用法:
#   # 自动下载安装 (推荐)
#   curl -sSL https://raw.githubusercontent.com/lteian/usb-autodump/web-version/INSTALL.sh | bash
#
#   # 或下载包后本地安装
#   tar -xzvf usb-autodump-*-synology-package.tar.gz
#   cd usb-autodump-*
#   chmod +x INSTALL.sh
#   ./INSTALL.sh
#
set -e

SCRIPT_VERSION="1.0.0"
PACKAGE_NAME="usb-autodump"
PACKAGE_DIR="/var/packages/${PACKAGE_NAME}"
TARGET_DIR="/var/packages/${PACKAGE_NAME}/target"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info()  { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# Detect if running on Synology
is_synology() {
    if [ -f /etc/synoinfo.conf ]; then
        return 0
    fi
    if command -v synouser >/dev/null 2>&1; then
        return 0
    fi
    return 1
}

# Get architecture
get_arch() {
    if [ -f /etc/synoinfo.conf ]; then
        grep "^unique=" /etc/synoinfo.conf | cut -d'"' -f2
    elif [ -f /etc/os-release ]; then
        grep "^ID=" /etc/os-release | cut -d= -f2
    else
        uname -m
    fi
}

# Detect Python
detect_python() {
    for py in python3 python3.11 python3.10 python3.9 python python3.8; do
        if command -v $py >/dev/null 2>&1; then
            PYTHON_BIN=$(command -v $py)
            PYTHON_VERSION=$($py --version 2>&1 | awk '{print $2}')
            return 0
        fi
    done
    return 1
}

# Install Python dependencies
install_deps() {
    log_info "安装 Python 依赖..."
    
    DEPS=("pyudev" "psutil" "fastapi" "uvicorn" "httpx" "python-multipart" "aiofiles" "websockets")
    
    for dep in "${DEPS[@]}"; do
        if $PYTHON_BIN -c "import ${dep%%[[:digit:]]*}" 2>/dev/null; then
            log_info "  [已有] $dep"
        else
            log_info "  [安装] $dep..."
            $PYTHON_BIN -m pip install --quiet "$dep" 2>/dev/null || \
            $PYTHON_BIN -m pip install "$dep"
        fi
    done
}

# Create package directory structure
create_dirs() {
    log_info "创建目录结构..."
    mkdir -p "${PACKAGE_DIR}"
    mkdir -p "${TARGET_DIR}"
    mkdir -p "${TARGET_DIR}/var"
    mkdir -p "${TARGET_DIR}/config"
    mkdir -p "${TARGET_DIR}/data"
    mkdir -p "${TARGET_DIR}/logs"
    mkdir -p "${TARGET_DIR}/scripts"
    mkdir -p "${TARGET_DIR}/python_deps"
    mkdir -p "${TARGET_DIR}/src"
}

# Copy files
copy_files() {
    log_info "复制文件..."
    
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    
    # Copy source
    if [ -d "${SCRIPT_DIR}/src" ]; then
        cp -r "${SCRIPT_DIR}/src/"* "${TARGET_DIR}/src/" 2>/dev/null || true
    fi
    
    # Copy scripts
    if [ -d "${SCRIPT_DIR}/spk_scripts/scripts" ]; then
        cp -r "${SCRIPT_DIR}/spk_scripts/scripts/"* "${TARGET_DIR}/scripts/"
    fi
    
    # Copy conf
    if [ -d "${SCRIPT_DIR}/spk_scripts/conf" ]; then
        mkdir -p "${TARGET_DIR}/conf"
        cp -r "${SCRIPT_DIR}/spk_scripts/conf/"* "${TARGET_DIR}/conf/" 2>/dev/null || true
    fi
    
    # Copy python_deps
    if [ -d "${SCRIPT_DIR}/python_deps" ]; then
        cp -r "${SCRIPT_DIR}/python_deps/"* "${TARGET_DIR}/python_deps/"
    fi
    
    # Copy version
    if [ -f "${SCRIPT_DIR}/VERSION" ]; then
        cp "${SCRIPT_DIR}/VERSION" "${TARGET_DIR}/"
    fi
}

# Create default config
create_config() {
    CONFIG_FILE="${TARGET_DIR}/config/config.json"
    if [ ! -f "$CONFIG_FILE" ]; then
        log_info "创建默认配置文件..."
        cat > "$CONFIG_FILE" << 'EOF'
{
    "ftp": {
        "host": "localhost",
        "port": 21,
        "username": "",
        "password": "",
        "remote_path": "/",
        " Passive mode": true
    },
    "usb": {
        "auto_mount": true,
        "eject_after_copy": false,
        "supported_fs": ["ntfs", "ext4", "exfat", "vfat", "hfsplus"]
    },
    "general": {
        "log_level": "INFO",
        "port": 8765,
        "auto_start": true
    },
    "copy": {
        "overwrite": false,
        "preserve_timestamps": true,
        "verify_copy": false
    }
}
EOF
        chmod 600 "$CONFIG_FILE"
    fi
}

# Set permissions
set_permissions() {
    log_info "设置权限..."
    chmod 755 "${TARGET_DIR}"
    chmod 755 "${TARGET_DIR}/scripts"
    chmod 755 "${TARGET_DIR}/scripts/"*.sh
    chmod 755 "${TARGET_DIR}/src"
    find "${TARGET_DIR}/src" -name "*.py" -exec chmod 644 {} \; 2>/dev/null || true
    chmod 755 "${TARGET_DIR}/var"
    chmod 755 "${TARGET_DIR}/config"
    chmod 755 "${TARGET_DIR}/data"
    chmod 755 "${TARGET_DIR}/logs"
}

# Register as Synology package (if on Synology)
register_synology_package() {
    if ! is_synology; then
        log_warn "非 Synology 环境，跳过包注册"
        return 0
    fi
    
    log_info "注册 Synology 包..."
    
    # Create INFO.json
    cat > "${PACKAGE_DIR}/INFO.json" << EOF
{
    "package": "${PACKAGE_NAME}",
    "version": "${SCRIPT_VERSION}",
    "arch": ["any"],
    "description": "USB 自动转储工具 - 自动检测并复制 USB 设备内容",
    "maintainer": "lteian",
    "install_deps": ["python3"],
    "startup": {
        "type": "startup",
        "script": "startup.sh"
    },
    "privileges": {
        "run-as": "package"
    }
}
EOF
    
    # Create PKG_DEPS
    echo "python3" > "${PACKAGE_DIR}/PKG_DEPS"
    
    log_info "Synology 包注册完成"
}

# Create systemd service (for non-Synology Linux)
create_systemd_service() {
    if is_synology; then
        return 0
    fi
    
    log_info "创建 systemd 服务..."
    
    SERVICE_FILE="/etc/systemd/system/${PACKAGE_NAME}.service"
    
    cat > "$SERVICE_FILE" << EOF
[Unit]
Description=USB Auto Dump Service
After=network.target

[Service]
Type=simple
User=root
WorkingDirectory=${TARGET_DIR}/src
Environment="PYTHONPATH=${TARGET_DIR}/python_deps:${TARGET_DIR}/src"
ExecStart=${PYTHON_BIN} -m uvicorn main:app --host 0.0.0.0 --port 8765
Restart=on-failure
RestartSec=10

[Install]
WantedBy=multi-user.target
EOF
    
    systemctl daemon-reload
    systemctl enable "${PACKAGE_NAME}.service"
    log_info "systemd 服务已启用"
}

# Start service
start_service() {
    log_info "启动服务..."
    
    if is_synology; then
        if [ -x "${TARGET_DIR}/scripts/startup.sh" ]; then
            "${TARGET_DIR}/scripts/startup.sh"
        fi
    else
        systemctl restart "${PACKAGE_NAME}.service" 2>/dev/null || \
        "${TARGET_DIR}/scripts/startup.sh" 2>/dev/null || \
        log_warn "无法自动启动服务，请手动启动"
    fi
}

# Print summary
print_summary() {
    echo ""
    echo "=========================================="
    echo -e "${GREEN}USB Auto Dump 安装完成!${NC}"
    echo "=========================================="
    echo ""
    echo "  包目录: ${TARGET_DIR}"
    echo "  配置:   ${TARGET_DIR}/config/config.json"
    echo "  日志:   ${TARGET_DIR}/logs/"
    echo "  端口:   8765"
    echo ""
    echo "  Web UI: http://<NAS-IP>:8765"
    echo ""
    echo "  常用命令:"
    echo "    启动:   ${TARGET_DIR}/scripts/startup.sh"
    echo "    停止:   ${TARGET_DIR}/scripts/stop.sh"
    echo "    重启:   ${TARGET_DIR}/scripts/stop.sh && ${TARGET_DIR}/scripts/startup.sh"
    echo "    卸载:   ${TARGET_DIR}/scripts/uninstaller.sh"
    echo ""
    
    if is_synology; then
        echo "  Synology 套件中心安装:"
        echo "    请在 套件中心 > 设置 > 套件来源 添加本仓库"
        echo "    然后搜索 usb-autodump 安装"
        echo ""
    fi
    echo "=========================================="
    echo ""
}

# Main
main() {
    echo "=========================================="
    echo "  USB Auto Dump 安装脚本 v${SCRIPT_VERSION}"
    echo "=========================================="
    echo ""
    
    if ! is_synology && [ "$(id -u)" -ne 0 ]; then
        log_error "请使用 root 权限运行"
        exit 1
    fi
    
    log_info "检测环境..."
    ARCH=$(get_arch)
    log_info "  架构: $ARCH"
    
    if detect_python; then
        log_info "  Python: $PYTHON_BIN ($PYTHON_VERSION)"
    else
        log_error "未找到 Python，请先安装 python3"
        exit 1
    fi
    
    create_dirs
    copy_files
    install_deps
    create_config
    set_permissions
    register_synology_package
    create_systemd_service
    start_service
    print_summary
}

# Handle arguments
case "${1:-}" in
    --help|-h)
        echo "用法: $0 [--help]"
        echo ""
        echo "  在 Synology NAS 上安装 USB Auto Dump"
        echo ""
        echo "  需要 root 权限运行"
        exit 0
        ;;
    *)
        main
        ;;
esac
