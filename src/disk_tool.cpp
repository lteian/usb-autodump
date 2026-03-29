#include "disk_tool.h"
#include <QProcess>
#include <QDebug>
#include <QCoreApplication>

#ifdef _WIN32
#include <windows.h>
#include <winioctl.h>
#else
#include <sys/statvfs.h>
#endif

bool DiskTool::formatDrive(const QString& drive, const QString& fs, const QString& label) {
    QString drv = drive;
    drv.replace("/", "").replace("\\", "");
    if (!drv.endsWith(":")) drv += ":";

#ifdef _WIN32
    QString script = QString("select volume %1\nformat fs=%2 label=\"%3\" quick\nassign\nexit\n")
                         .arg(drv)
                         .arg(fs)
                         .arg(label);

    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start("diskpart", QStringList() << "/s" << script);
    p.waitForFinished(120000);
    if (p.exitCode() != 0) {
        qWarning() << "format failed:" << p.readAll();
        return false;
    }
    return true;
#else
    // Linux: assume drv is device path like /dev/sdb1
    QString device = drv;
    if (!device.startsWith("/dev/")) device = "/dev/" + device;
    QProcess p;
    QStringList args;
    if (fs.toUpper() == "FAT32" || fs.toUpper() == "VFAT") {
        args << "-n" << label << device;
    } else if (fs.toUpper() == "NTFS") {
        args << "-n" << label << device;
    } else {
        args << "-L" << label << device;
    }
    p.start("mkfs.vfat", args);
    p.waitForFinished(120000);
    return p.exitCode() == 0;
#endif
}

bool DiskTool::ejectDrive(const QString& drive) {
    QString drv = drive;
    drv.replace("/", "").replace("\\", "");
    if (!drv.endsWith(":")) {
        if (drv.length() == 1) drv += ":";
    }

#ifdef _WIN32
    // Try DeviceIoControl to eject
    QString volPath = QString("\\\\.\\%1:").arg(drv);
    HANDLE h = CreateFileW(
        (LPCWSTR)volPath.utf16(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    if (h != INVALID_HANDLE_VALUE) {
        BOOL ok = DeviceIoControl(h, 0x000900a8, NULL, 0, NULL, 0, NULL, NULL);
        CloseHandle(h);
        if (ok) return true;
    }
    // Fallback: use mountvol
    QProcess p;
    p.start("mountvol", QStringList() << drv + "\\" << "/P");
    p.waitForFinished(30000);
    return p.exitCode() == 0;
#else
    QString device = drv;
    if (!device.startsWith("/dev/")) device = "/dev/" + device;
    QProcess p;
    p.start("umount", QStringList() << device);
    p.waitForFinished(30000);
    return p.exitCode() == 0;
#endif
}

bool DiskTool::getDiskSpace(const QString& path, qint64& freeBytes, qint64& totalBytes) {
#ifdef _WIN32
    ULARGE_INTEGER freeBytesLarge, totalBytesLarge, totalFreeBytesLarge;
    // Extract root from path, e.g. "D:/folder" -> "D:\"
    QString root = path.mid(0, 2);
    root += "\\";
    if (GetDiskFreeSpaceExW((LPCWSTR)root.utf16(),
                           &freeBytesLarge, &totalBytesLarge, &totalFreeBytesLarge)) {
        freeBytes = freeBytesLarge.QuadPart;
        totalBytes = totalBytesLarge.QuadPart;
        return true;
    }
    return false;
#else
    struct statvfs fs;
    if (statvfs(path.toUtf8().constData(), &fs) == 0) {
        freeBytes = (qint64)fs.f_bavail * fs.f_frsize;
        totalBytes = (qint64)fs.f_blocks * fs.f_frsize;
        return true;
    }
    return false;
#endif
}
