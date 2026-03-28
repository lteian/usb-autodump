#include "file_record.h"
#include <QDir>
#include <QStandardPaths>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariant>
#include <QFile>
#include <QDebug>
#include <QDateTime>

class FileRecordDB::DB {
public:
    QSqlDatabase db;
};

FileRecordDB::FileRecordDB() {
    d = new DB;
    d->db = QSqlDatabase::addDatabase("QSQLITE", "file_records");
    d->db.setDatabaseName(dbPath());
    d->db.open();
    ensureTable();
}

FileRecordDB& FileRecordDB::instance() {
    static FileRecordDB inst;
    return inst;
}

QString FileRecordDB::dbPath() {
    return QDir(QStandardPaths::writableLocation(QStandardPaths::DataLocation))
               .filePath("usb-autodump/file_records.db");
}

void FileRecordDB::ensureTable() {
    QSqlQuery q(d->db);
    q.exec("CREATE TABLE IF NOT EXISTS file_records ("
           "id INTEGER PRIMARY KEY AUTOINCREMENT,"
           "usb_drive TEXT NOT NULL,"
           "file_path TEXT NOT NULL,"
           "local_path TEXT NOT NULL,"
           "ftp_path TEXT,"
           "status TEXT NOT NULL DEFAULT 'pending',"
           "file_size INTEGER DEFAULT 0,"
           "copied_at TEXT,"
           "uploaded_at TEXT,"
           "deleted_at TEXT,"
           "error_msg TEXT"
           ")");
    q.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_drive_file ON file_records(usb_drive, file_path)");
}

int FileRecordDB::add(const FileRecord& r) {
    QSqlQuery q(d->db);
    q.prepare("INSERT OR REPLACE INTO file_records "
              "(usb_drive, file_path, local_path, status, file_size, copied_at) "
              "VALUES (?,?,?,?,?,datetime('now'))");
    q.bindValue(0, r.usbDrive);
    q.bindValue(1, r.filePath);
    q.bindValue(2, r.localPath);
    q.bindValue(3, "pending");
    q.bindValue(4, r.fileSize);
    q.exec();
    return q.lastInsertId().toInt();
}

void FileRecordDB::updateStatus(int id, const QString& status, const QString& err) {
    QSqlQuery q(d->db);
    QString ts = QDateTime::currentDateTime().toString(Qt::ISODate);
    if (status == "copied") {
        q.exec(QString("UPDATE file_records SET status='copied',copied_at='%1' WHERE id=%2").arg(ts).arg(id));
    } else if (status == "uploaded") {
        q.exec(QString("UPDATE file_records SET status='uploaded',uploaded_at='%1' WHERE id=%2").arg(ts).arg(id));
    } else if (status == "deleted") {
        q.exec(QString("UPDATE file_records SET status='deleted',deleted_at='%1' WHERE id=%2").arg(ts).arg(id));
    } else if (status == "error") {
        q.exec(QString("UPDATE file_records SET status='error',error_msg='%1' WHERE id=%2").arg(err).arg(id));
    } else {
        q.exec(QString("UPDATE file_records SET status='%1' WHERE id=%2").arg(status).arg(id));
    }
}

void FileRecordDB::updateFtpPath(int id, const QString& path) {
    QSqlQuery q(d->db);
    q.exec(QString("UPDATE file_records SET ftp_path='%1' WHERE id=%2").arg(path).arg(id));
}

QList<FileRecord> FileRecordDB::pendingRecords() {
    QList<FileRecord> list;
    QSqlQuery q(d->db);
    q.exec("SELECT * FROM file_records WHERE status IN ('pending','copying','copied','uploading')");
    while (q.next()) {
        FileRecord r;
        r.id = q.value("id").toInt();
        r.usbDrive = q.value("usb_drive").toString();
        r.filePath = q.value("file_path").toString();
        r.localPath = q.value("local_path").toString();
        r.ftpPath = q.value("ftp_path").toString();
        r.status = q.value("status").toString();
        r.fileSize = q.value("file_size").toInt();
        r.copiedAt = q.value("copied_at").toString();
        r.uploadedAt = q.value("uploaded_at").toString();
        r.deletedAt = q.value("deleted_at").toString();
        r.errorMsg = q.value("error_msg").toString();
        list << r;
    }
    return list;
}

QList<FileRecord> FileRecordDB::recordsForDrive(const QString& drive) {
    QList<FileRecord> list;
    QSqlQuery q(d->db);
    q.exec(QString("SELECT * FROM file_records WHERE usb_drive='%1'").arg(drive));
    while (q.next()) {
        FileRecord r;
        r.id = q.value("id").toInt();
        r.usbDrive = q.value("usb_drive").toString();
        r.filePath = q.value("file_path").toString();
        r.localPath = q.value("local_path").toString();
        r.ftpPath = q.value("ftp_path").toString();
        r.status = q.value("status").toString();
        r.fileSize = q.value("file_size").toInt();
        r.copiedAt = q.value("copied_at").toString();
        r.uploadedAt = q.value("uploaded_at").toString();
        r.deletedAt = q.value("deleted_at").toString();
        r.errorMsg = q.value("error_msg").toString();
        list << r;
    }
    return list;
}

QPair<int, qint64> FileRecordDB::pendingCountAndSize() {
    QSqlQuery q(d->db);
    q.exec("SELECT COUNT(*), COALESCE(SUM(file_size),0) FROM file_records "
           "WHERE status IN ('pending','copying','copied','uploading')");
    if (q.next()) return qMakePair(q.value(0).toInt(), q.value(1).toLongLong());
    return qMakePair(0, Q_INT64_C(0));
}

QPair<int, qint64> FileRecordDB::uploadedCountAndSize() {
    QSqlQuery q(d->db);
    q.exec("SELECT COUNT(*), COALESCE(SUM(file_size),0) FROM file_records "
           "WHERE status IN ('uploaded','deleted')");
    if (q.next()) return qMakePair(q.value(0).toInt(), q.value(1).toLongLong());
    return qMakePair(0, Q_INT64_C(0));
}

bool FileRecordDB::hasPendingUpload(const QString& localPath) {
    QSqlQuery q(d->db);
    q.prepare("SELECT status FROM file_records WHERE local_path=?");
    q.bindValue(0, localPath);
    q.exec();
    if (q.next()) {
        QString s = q.value(0).toString();
        // Already queued or done - skip
        if (s == "pending" || s == "uploading" || s == "copied" || s == "uploaded")
            return true;
    }
    return false;
}

void FileRecordDB::clearAll() {
    QSqlQuery q(d->db);
    q.exec("DELETE FROM file_records");
    QFile::remove(dbPath());
}
