#ifndef FTP_PROCESS_H
#define FTP_PROCESS_H

#include <QObject>
#include <QString>
#include <QProcess>
#include <QJsonObject>
#include <QList>
#include <QMutex>

// FTP task structure
struct FTPUploadTask {
    int recordId = 0;
    QString localPath;
    QString remotePath;
    qint64 fileSize = 0;
    QString status;
};

class FTPProcess : public QObject {
    Q_OBJECT
public:
    explicit FTPProcess(QObject* parent = nullptr);
    ~FTPProcess();

    // Send upload task to subprocess
    void sendUpload(const FTPUploadTask& task);
    void sendCancelAll();

    bool isConnected() const { return m_connected; }

signals:
    void uploadProgress(int recordId, qint64 uploadedBytes, qint64 totalBytes);
    void uploadDone(int recordId);
    void fileDeleted(int recordId);
    void uploadError(int recordId, const QString& msg);
    void connectedChanged(bool connected);
    void logMessage(const QString& msg);

private slots:
    void onReadyRead();
    void onFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    void startSubprocess();
    void handleJson(const QJsonObject& obj);

    QProcess* m_process = nullptr;
    QMutex m_mutex;
    QList<FTPUploadTask> m_pendingQueue;
    bool m_started = false;
    bool m_connected = false;
};

#endif // FTP_PROCESS_H
