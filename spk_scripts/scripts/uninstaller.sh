#!/bin/bash
#
# uninstaller.sh - Uninstall usb-autodump package
#

PACKAGE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && cd .. && pwd)"
SERVICE_USER="usb-autodump"
KEEP_CONFIG="${KEEP_CONFIG:-yes}"

echo "[uninstaller] Uninstalling usb-autodump..."

# Stop service if running
echo "[uninstaller] Stopping service..."
if [ -f "${PACKAGE_DIR}/scripts/stop.sh" ]; then
    "${PACKAGE_DIR}/scripts/stop.sh" 2>/dev/null || true
fi

# Kill any remaining processes
pkill -f "uvicorn.*usb-autodump" 2>/dev/null || true
pkill -f "usb-autodump" 2>/dev/null || true

# Remove user (if exists)
if command -v synouser >/dev/null 2>&1; then
    synouser --del "${SERVICE_USER}" 2>/dev/null || true
fi

# Handle config/data
if [ "${KEEP_CONFIG}" = "yes" ]; then
    echo "[uninstaller] Keeping config and data (KEEP_CONFIG=yes)"
else
    echo "[uninstaller] Removing config and data..."
    rm -rf "${PACKAGE_DIR}/config"
    rm -rf "${PACKAGE_DIR}/data"
fi

# Clean logs and var
echo "[uninstaller] Cleaning temporary files..."
rm -rf "${PACKAGE_DIR}/var"/*.log "${PACKAGE_DIR}/var"/*.pid "${PACKAGE_DIR}/var"/*.tmp 2>/dev/null || true
rm -rf "${PACKAGE_DIR}/logs"/* 2>/dev/null || true

echo "[uninstaller] Uninstallation complete!"
