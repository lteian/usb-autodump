#ifndef DISK_TOOL_H
#define DISK_TOOL_H

#include <QString>
#include <QObject>
#include <QThread>
#include <QMutex>

class DiskTool : public QObject {
    Q_OBJECT
public:
    // Format a drive (e.g. "E:" on Windows, "/dev/sdb1" on Linux)
    // fs: FAT32/NTFS/EXT4, label: volume label
    // WARNING: This is a blocking call - use asyncFormatDrive for non-blocking
    static bool formatDrive(const QString& drive, const QString& fs = "FAT32", const QString& label = "USB");

    // Async version - runs in separate thread, emits finished in main thread
    static void asyncFormatDrive(const QString& drive, const QString& fs = "FAT32", const QString& label = "USB");

    // Safely eject/unmount a drive
    static bool ejectDrive(const QString& drive);

    // Get free/total space for a path (extracts drive letter on Windows)
    static bool getDiskSpace(const QString& path, qint64& freeBytes, qint64& totalBytes);

signals:
    void formatFinished(const QString& drive, bool success, const QString& error);

private:
    static QThread* s_workerThread;
    static QMutex s_mutex;
};

// Global accessor for DiskTool signals
DiskTool* diskToolInstance();

#endif // DISK_TOOL_H