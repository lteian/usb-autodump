; USB-AutoDump NSIS Installer
!include "MUI2.nsh"
!include "FileFunc.nsh"

!define VER "1.0.0"

Name "USB-AutoDump v${VER}"
OutFile "USB-AutoDump-Setup.exe"
InstallDir "$PROGRAMFILES\USB-AutoDump"
InstallDirRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\App Paths\usb-autodump.exe" ""
ShowInstDetails show
ShowUnInstDetails show
CRCCheck on
SetCompressor /SOLID lzma

!define MUI_ICON "icon.ico"
!define MUI_UNICON "icon.ico"

!insertmacro MUI_LANGUAGE "English"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "license.txt"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

Section "Main Application" SEC01
  SetOutPath "$INSTDIR"
  SetOverwrite on
  File /r "dist\2026-03-27"
SectionEnd

Section -Post
  WriteUninstaller "$INSTDIR\uninst.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\App Paths\usb-autodump.exe" "" "$INSTDIR\usb-autodump.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\USB-AutoDump" "DisplayName" "USB-AutoDump"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\USB-AutoDump" "UninstallString" "$INSTDIR\uninst.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\USB-AutoDump" "DisplayIcon" "$INSTDIR\usb-autodump.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\USB-AutoDump" "Publisher" "ZhengZhong"
SectionEnd

Section "Uninstall"
  ExecWait "taskkill /F /IM usb-autodump.exe"
  Sleep 1000
  RMDir /r "$INSTDIR"
  RMDir /r "$SMPROGRAMS\USB-AutoDump"
  Delete "$DESKTOP\USB-AutoDump.lnk"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\USB-AutoDump"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\App Paths\usb-autodump.exe"
  SetAutoClose true
SectionEnd