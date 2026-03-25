#!/usr/bin/env python3
"""
setup_env.py - Environment preparation for Synology NAS

This script:
1. Checks dependencies
2. Installs Python packages using pip install --target
3. Compatible with Synology's Python environment
"""

import sys
import os
import subprocess
import site


def run_cmd(cmd, check=True):
    """Run shell command, return output."""
    try:
        result = subprocess.run(
            cmd, shell=True, capture_output=True, text=True
        )
        if check and result.returncode != 0:
            print(f"[setup] WARNING: '{cmd}' failed: {result.stderr.strip()}")
        return result
    except Exception as e:
        print(f"[setup] ERROR running '{cmd}': {e}")
        return None


def ensure_pip():
    """Ensure pip is available."""
    print("[setup] Checking pip...")

    # Try python3 -m pip first
    result = run_cmd(f"{sys.executable} -m pip --version", check=False)
    if result and result.returncode == 0:
        print(f"[setup] pip found: {result.stdout.strip()}")
        return True

    # Try ensurepip
    print("[setup] Trying ensurepip...")
    result = run_cmd(f"{sys.executable} -m ensurepip --upgrade", check=False)
    if result and result.returncode == 0:
        return True

    # Download get-pip.py
    print("[setup] Downloading pip...")
    result = run_cmd(
        f"curl -sS https://bootstrap.pypa.io/get-pip.py | {sys.executable}"
    )
    if result and result.returncode == 0:
        return True

    return False


def install_package(pkg, target_dir):
    """Install a single package to target directory."""
    print(f"[setup] Installing {pkg}...")
    cmd = (
        f"{sys.executable} -m pip install --target={target_dir} "
        f"--quiet --upgrade {pkg}"
    )
    result = run_cmd(cmd, check=False)
    if result and result.returncode == 0:
        print(f"[setup]   {pkg} installed")
        return True
    else:
        # Try without --quiet
        cmd = (
            f"{sys.executable} -m pip install --target={target_dir} "
            f"--upgrade {pkg}"
        )
        result = run_cmd(cmd, check=False)
        return result and result.returncode == 0


def check_required_modules():
    """Check if required modules are importable."""
    required = [
        ("pyudev", "pyudev"),
        ("psutil", "psutil"),
        ("fastapi", "fastapi"),
        ("uvicorn", "uvicorn"),
    ]

    missing = []
    for mod_name, import_name in required:
        try:
            __import__(import_name)
            print(f"[setup] {mod_name}: OK")
        except ImportError:
            print(f"[setup] {mod_name}: MISSING")
            missing.append(mod_name)

    return missing


def main():
    print(f"[setup] Python: {sys.executable}")
    print(f"[setup] Version: {sys.version}")

    # Get package directory (parent of scripts/)
    script_dir = os.path.dirname(os.path.abspath(__file__))
    pkg_dir = os.path.dirname(script_dir)
    target_dir = os.path.join(pkg_dir, "python_deps")

    os.makedirs(target_dir, exist_ok=True)
    print(f"[setup] Target directory: {target_dir}")

    # Ensure pip
    if not ensure_pip():
        print("[setup] FATAL: Cannot install pip")
        return 1

    # Add target to Python path
    site.addsitedir(target_dir)
    sys.path.insert(0, target_dir)

    # Check what's missing
    missing = check_required_modules()

    # Install missing packages
    if missing:
        print(f"[setup] Installing {len(missing)} missing packages...")
        for pkg in missing:
            install_package(pkg, target_dir)
    else:
        print("[setup] All required modules available")

    # Final check
    print("[setup] Final verification...")
    missing = check_required_modules()
    if missing:
        print(f"[setup] WARNING: Still missing: {missing}")
        return 1

    print("[setup] Environment ready!")
    return 0


if __name__ == "__main__":
    sys.exit(main())
