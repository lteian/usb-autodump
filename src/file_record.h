#ifndef FILE_RECORD_H
#define FILE_RECORD_H

#include <QString>
#include <QList>
#include <QPair>

struct FileRecord {
    int id = 0;
    QString usbDrive;
    QString filePath;
    QString localPath;
    QString ftpPath;
    QString status; // pending/copying/copied/uploading/uploaded/deleted/error
    qint64 fileSize = 0;
    QString copiedAt;
    QString uploadedAt;
    QString deletedAt;
    QString errorMsg;
};

class FileRecordDB {
public:
    static FileRecordDB& instance();

    int add(const FileRecord& r);
    void updateStatus(int id, const QString& status, const QString& err = "");
    void updateFtpPath(int id, const QString& path);
    QList<FileRecord> pendingRecords();
    QList<FileRecord> recordsForDrive(const QString& drive);
    QPair<int, qint64> pendingCountAndSize();
    QPair<int, qint64> uploadedCountAndSize();
    bool hasPendingUpload(const QString& localPath);
    void clearAll();

private:
    FileRecordDB();
    void ensureTable();
    QString dbPath();

    class DB;
    DB* d;
};

#endif // FILE_RECORD_H
