#!/bin/bash
#
# startup.sh - Start usb-autodump service
#

PACKAGE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && cd .. && pwd)"
APP_DIR="${PACKAGE_DIR}/app"
PYTHON_DEPS="${PACKAGE_DIR}/python_deps"
PID_FILE="${PACKAGE_DIR}/var/usb-autodump.pid"
LOG_FILE="${PACKAGE_DIR}/var/usb-autodump.log"
PORT=8765

# Create necessary directories
mkdir -p "${PACKAGE_DIR}/var"
mkdir -p "${PACKAGE_DIR}/config"
mkdir -p "${PACKAGE_DIR}/data"

echo "[startup] Starting usb-autodump..."

# Check Python3
PYTHON_BIN=""
for py in python3 python python3.11 python3.10 python3.9 python3.8; do
    if command -v $py >/dev/null 2>&1; then
        PYTHON_BIN=$(command -v $py)
        break
    fi
done

if [ -z "${PYTHON_BIN}" ]; then
    echo "[startup] ERROR: python3 not found"
    exit 1
fi

echo "[startup] Using Python: ${PYTHON_BIN}"

# Ensure pip is available
if ! ${PYTHON_BIN} -m pip --version >/dev/null 2>&1; then
    echo "[startup] Installing pip..."
    ${PYTHON_BIN} -m ensurepip --upgrade 2>/dev/null || \
    curl -sS https://bootstrap.pypa.io/get-pip.py | ${PYTHON_BIN} --user
fi

# Setup environment with bundled deps
export PYTHONPATH="${PYTHON_DEPS}:${PYTHONPATH}"
export PATH="${PYTHON_DEPS}/bin:${PATH}"

# Check if already running
if [ -f "${PID_FILE}" ]; then
    OLD_PID=$(cat "${PID_FILE}")
    if kill -0 "${OLD_PID}" 2>/dev/null; then
        echo "[startup] Service already running (PID ${OLD_PID})"
        exit 0
    else
        rm -f "${PID_FILE}"
    fi
fi

# Start the service
echo "[startup] Starting FastAPI server on port ${PORT}..."

cd "${APP_DIR}"

nohup ${PYTHON_BIN} -m uvicorn main:app \
    --host 0.0.0.0 \
    --port ${PORT} \
    --log-level info \
    > "${LOG_FILE}" 2>&1 &

echo $! > "${PID_FILE}"
echo "[startup] Started with PID $(cat ${PID_FILE})"
echo "[startup] Log: ${LOG_FILE}"
