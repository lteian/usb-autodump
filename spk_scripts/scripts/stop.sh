#!/bin/bash
#
# stop.sh - Stop usb-autodump service gracefully
#

PACKAGE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && cd .. && pwd)"
PID_FILE="${PACKAGE_DIR}/var/usb-autodump.pid"
TIMEOUT=10

echo "[stop] Stopping usb-autodump..."

# Function to cleanup
cleanup() {
    echo "[stop] Cleaning up temp files..."
    rm -f "${PACKAGE_DIR}/var/"*.tmp 2>/dev/null
    rm -f "${PACKAGE_DIR}/var/"*.lock 2>/dev/null
}

# Function to stop process
do_stop() {
    if [ -f "${PID_FILE}" ]; then
        PID=$(cat "${PID_FILE}")
        if kill -0 "${PID}" 2>/dev/null; then
            echo "[stop] Sending SIGTERM to ${PID}..."
            kill "${PID}" 2>/dev/null
            
            # Wait for graceful shutdown
            for i in $(seq 1 ${TIMEOUT}); do
                if ! kill -0 "${PID}" 2>/dev/null; then
                    echo "[stop] Process stopped gracefully"
                    rm -f "${PID_FILE}"
                    cleanup
                    return 0
                fi
                sleep 1
            done
            
            # Force kill if still running
            echo "[stop] Force killing..."
            kill -9 "${PID}" 2>/dev/null
            rm -f "${PID_FILE}"
        else
            echo "[stop] PID file exists but process not running"
            rm -f "${PID_FILE}"
        fi
    else
        echo "[stop] No PID file found, trying to find process..."
        pkill -f "uvicorn.*usb-autodump" 2>/dev/null || true
        pkill -f "usb-autodump" 2>/dev/null || true
    fi
    
    cleanup
    echo "[stop] Done"
    return 0
}

do_stop
exit 0
