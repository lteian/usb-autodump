#!/bin/bash
#
# build_spk.sh - Build Synology SPK package for usb-autodump
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PKG_DIR="${SCRIPT_DIR}"
DIST_DIR="${PKG_DIR}/dist"
PYTHON_DEPS_DIR="${DIST_DIR}/python_deps"

# Package info
PKG_NAME="usb-autodump"
PKG_VERSION=$(grep '"version"' "${PKG_DIR}/PACKAGE.json" | sed 's/.*"version": "\([^"]*\)".*/\1/')
SPK_NAME="${PKG_NAME}-${PKG_VERSION}"
ARCH="${ARCH:-x86_64}"

echo "=== Building ${SPK_NAME} for arch=${ARCH} ==="

# Clean previous build
rm -rf "${DIST_DIR}"
mkdir -p "${DIST_DIR}"

# Step 1: Install Python dependencies to isolated directory
echo "[1/5] Installing Python dependencies..."
mkdir -p "${PYTHON_DEPS_DIR}"

# Detect python3 and ensure pip is available
PYTHON_BIN=$(command -v python3 || command -v python)
if [ -z "${PYTHON_BIN}" ]; then
    echo "ERROR: python3 not found"
    exit 1
fi

# Install pip if not present
if ! ${PYTHON_BIN} -m pip --version >/dev/null 2>&1; then
    echo "Installing pip..."
    ${PYTHON_BIN} -m ensurepip --upgrade 2>/dev/null || \
    curl -sS https://bootstrap.pypa.io/get-pip.py | ${PYTHON_BIN}
fi

# Install required packages to target directory
DEPS=("pyudev" "psutil" "fastapi" "uvicorn" "httpx" "python-multipart")
for dep in "${DEPS[@]}"; do
    echo "  Installing ${dep}..."
    ${PYTHON_BIN} -m pip install --target="${PYTHON_DEPS_DIR}" --quiet "${dep}" 2>/dev/null || \
    ${PYTHON_BIN} -m pip install --target="${PYTHON_DEPS_DIR}" "${dep}"
done

# Step 2: Copy application files
echo "[2/5] Copying application files..."
mkdir -p "${DIST_DIR}/app"
cp -r "${PKG_DIR}/../src" "${DIST_DIR}/app/" 2>/dev/null || true
cp -r "${PKG_DIR}/scripts" "${DIST_DIR}/"
mkdir -p "${DIST_DIR}/conf"
cp "${PKG_DIR}/conf/privilege" "${DIST_DIR}/conf/" 2>/dev/null || true
cp "${PKG_DIR}/PACKAGE.json" "${DIST_DIR}/"

# Copy Python deps
cp -r "${PYTHON_DEPS_DIR}" "${DIST_DIR}/"

# Step 3: Generate INFO.json (Synology format)
echo "[3/5] Generating INFO.json..."
cat > "${DIST_DIR}/INFO.json" << 'EOF'
{
    "package": "usb-autodump",
    "version": "1.0.0",
    "arch": ["broadwell", "broadwellnk", "v1000", "geminilake", "x86_64"],
    "description": "USB auto dump tool with FTP upload",
    "maintainer": "lteian",
    "install_deps": ["python3"],
    "startup": {
        "type": "startup",
        "script": "startup.sh"
    },
    "privileges": {
        "run-as": "package",
        "port-forward": false
    }
}
EOF

# Step 4: Create tar.gz
echo "[4/5] Creating tar.gz..."
cd "${DIST_DIR}"
tar -czf "${SPK_NAME}.tar.gz" \
    --exclude='*.tar.gz' \
    --exclude='.DS_Store' \
    .

# Step 5: Package as SPK (tar.gz with .spk extension + meta)
mv "${SPK_NAME}.tar.gz" "${SPK_NAME}.spk"

echo "[5/5] SPK created: ${DIST_DIR}/${SPK_NAME}.spk"
echo "=== Build complete ==="
