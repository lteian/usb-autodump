#!/bin/bash
#
# installer.sh - Install usb-autodump package
#

PACKAGE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && cd .. && pwd)"
SERVICE_USER="usb-autodump"
SERVICE_GROUP="nobody"

echo "[installer] Installing usb-autodump..."

# Create package directories
echo "[installer] Creating directories..."
mkdir -p "${PACKAGE_DIR}/var"
mkdir -p "${PACKAGE_DIR}/config"
mkdir -p "${PACKAGE_DIR}/data"
mkdir -p "${PACKAGE_DIR}/logs"

# Create system user (if not exists)
if ! synouser --get "${SERVICE_USER}" >/dev/null 2>&1; then
    echo "[installer] Creating user ${SERVICE_USER}..."
    # Synology uses special user creation
    if command -v synouser >/dev/null 2>&1; then
        synouser --add "${SERVICE_USER}" "" "USB Auto Dump User" 0 "" 0 2>/dev/null || true
    fi
fi

# Set directory permissions
echo "[installer] Setting permissions..."
chmod 755 "${PACKAGE_DIR}"
chmod 755 "${PACKAGE_DIR}/var"
chmod 755 "${PACKAGE_DIR}/config"
chmod 755 "${PACKAGE_DIR}/data"
chmod 755 "${PACKAGE_DIR}/logs"

# Create default config if not exists
CONFIG_FILE="${PACKAGE_DIR}/config/config.json"
if [ ! -f "${CONFIG_FILE}" ]; then
    echo "[installer] Creating default config..."
    cat > "${CONFIG_FILE}" << 'EOF'
{
    "ftp": {
        "host": "localhost",
        "port": 21,
        "username": "",
        "password": "",
        "remote_path": "/"
    },
    "usb": {
        "auto_mount": true,
        "eject_after_copy": false
    },
    "general": {
        "log_level": "INFO",
        "port": 8765
    }
}
EOF
    chmod 600 "${CONFIG_FILE}"
fi

# Run environment setup
echo "[installer] Running environment setup..."
${PACKAGE_DIR}/scripts/setup_env.py

echo "[installer] Installation complete!"
echo "[installer] Run 'startup.sh' to start the service"
