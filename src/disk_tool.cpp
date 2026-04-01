#include "disk_tool.h"
#include <QProcess>
#include <QDebug>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryFile>
#include <QTimer>

#ifdef _WIN32
#include <windows.h>
#include <winioctl.h>
#else
#include <sys/statvfs.h>
#endif

QThread* DiskTool::s_workerThread = nullptr;
QMutex DiskTool::s_mutex;

// Global instance to emit signals from main thread
static DiskTool* s_instance = nullptr;

DiskTool* diskToolInstance() {
    if (!s_instance) {
        s_instance = new DiskTool();
    }
    return s_instance;
}

bool DiskTool::formatDrive(const QString& drive, const QString& fs, const QString& label) {
    QString drv = drive;
    drv.replace("/", "").replace("\\", "");
    if (!drv.endsWith(":")) drv += ":";

#ifdef _WIN32
    // Use PowerShell's Format-Volume - works well in scripts, no admin needed for quick format
    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    // Extract drive letter without colon (e.g., "D")
    QString driveLetter = drv;
    driveLetter.chop(1); // remove trailing colon
    QString psCmd = QString("Format-Volume -DriveLetter %1 -FileSystem %2 -Confirm:$false -Force")
                        .arg(driveLetter)
                        .arg(fs.toUpper());
    p.start("powershell.exe", QStringList() << "-NoProfile" << "-Command" << psCmd);
    // Format may take up to 2 minutes
    bool finished = p.waitForFinished(120000);
    if (!finished) {
        p.kill();
        qWarning() << "format timed out";
        return false;
    }
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

// Helper class for async formatting
class FormatRunner : public QObject {
    Q_OBJECT
public:
    FormatRunner(const QString& drive, const QString& fs, const QString& label)
        : m_drive(drive), m_fs(fs), m_label(label) {}
public slots:
    void run() {
        bool ok = DiskTool::formatDrive(m_drive, m_fs, m_label);
        QString error;
        if (!ok) {
            error = "格式化失败";
        }
        emit finished(m_drive, ok, error);
        this->deleteLater();
    }
signals:
    void finished(const QString& drive, bool ok, const QString& error);
private:
    QString m_drive, m_fs, m_label;
};

void DiskTool::asyncFormatDrive(const QString& drive, const QString& fs, const QString& label) {
    QMutexLocker locker(&s_mutex);

    if (!s_workerThread) {
        s_workerThread = new QThread();
        s_workerThread->start();
    }

    FormatRunner* runner = new FormatRunner(drive, fs, label);
    runner->moveToThread(s_workerThread);
    // Use QueuedConnection so signal is delivered to main thread
    connect(runner, &FormatRunner::finished, diskToolInstance(), &DiskTool::formatFinished, Qt::QueuedConnection);
    QTimer::singleShot(0, runner, SLOT(run()));
}

#include "disk_tool.moc"