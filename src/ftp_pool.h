#ifndef FTP_POOL_H
#define FTP_POOL_H

#include <QObject>
#include <QQueue>
#include <QMutex>
#include <QVector>
#include "ftp_process.h"

class FTPConnectionPool : public QObject {
    Q_OBJECT
public:
    explicit FTPConnectionPool(QObject* parent = nullptr, int maxConnections = 5);
    ~FTPConnectionPool();

    void sendUpload(const FTPUploadTask& task);
    void sendCancelAll();

signals:
    void uploadProgress(int recordId, qint64 uploadedBytes, qint64 totalBytes);
    void uploadDone(int recordId);
    void fileDeleted(int recordId, const QString& localPath);
    void uploadError(int recordId, const QString& msg);
    void connectedChanged(bool connected);
    void logMessage(const QString& msg);

private slots:
    void onUploadDone(int recordId);
    void onUploadError(int recordId, const QString& msg);
    void onUploadProgress(int recordId, qint64 uploadedBytes, qint64 totalBytes);
    void onConnectedChanged(bool connected);
    void onFileDeleted(int recordId, const QString& localPath);
    void onLogMessage(const QString& msg);

private:
    void assignTaskToWorker();

    QVector<FTPProcess*> m_workers;
    QQueue<FTPUploadTask> m_pendingQueue;
    QMutex m_mutex;
    int m_maxConnections;
    int m_workerIndex = 0;
};

#endif // FTP_POOL_H