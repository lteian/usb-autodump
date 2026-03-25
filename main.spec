# -*- mode: python ; coding: utf-8 -*-

from PyInstaller.building.build_main import main as pyi_main

# 让 PyInstaller 自动找所有 PyQt6 动态库
block_cipher = None

a = Analysis(
    ['main.py'],
    binaries=[],
    datas=[],
    hiddenimports=[
        'PyQt6.QtCore', 'PyQt6.QtWidgets', 'PyQt6.QtGui',
        'cryptography', 'cryptography.x509', 'cryptography.hazmat.bindings',
    ],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=['test', 'tkinter', 'matplotlib'],
    win_no_prefer_redirects=False,
    win_private_assemblies=True,
    cipher=block_cipher,
    noarchive=False,
)

pyz = PYZ(a.pure, a.zipped_data, cipher=block_cipher)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.zipfiles,
    a.datas,
    [],
    name='USB自动转储工具',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    console=False,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    icon=None,
)
