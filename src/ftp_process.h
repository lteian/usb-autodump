#ifndef FTP_PROCESS_H
#define FTP_PROCESS_H

#include <QObject>
#include <QString>
#include <QQueue>
#include <QMutex>
#include <QTcpSocket>
#include <QFile>

struct FTPUploadTask {
    int recordId = 0;
    QString localPath;
    QString remotePath;
    qint64 fileSize = 0;
    QString status;
};

enum FTPState {
    Idle,
    Connecting,
    Authenticating,
    Connected,
    WaitingPASV,
    WaitingSTOR,
    Uploading,
    Disconnecting
};

class FTPProcess : public QObject {
    Q_OBJECT
public:
    explicit FTPProcess(QObject* parent = nullptr);
    ~FTPProcess();

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
    void onConnected();
    void onCmdReadyRead();
    void onDisconnected();
    void onSocketError(QAbstractSocket::SocketError err);
    void onDataConnected();
    void onDataBytesWritten(qint64 bytes);
    void onDataDisconnected();

private:
    void processNext();
    void connectToHost();
    void enterPassiveMode();
    void ensureRemoteDir(const QString& remotePath);
    void sendMkdirSync(const QString& path);
    void enterPassiveModeStep2();
    void startUpload();
    void sendFileData();
    void finishUpload();
    void cleanupAndNext();
    void disconnectFromHost();
    void handleResponse(const QString& line);
    void removeEmptyParentDirs(const QString& filePath);

    QQueue<FTPUploadTask> m_queue;
    QMutex m_mutex;
    FTPUploadTask* m_currentTask = nullptr;

    QTcpSocket* m_cmdSocket = nullptr;
    QTcpSocket* m_dataSocket = nullptr;

    QString m_dataHost;
    int m_dataPort = 0;
    QString m_user;
    QString m_pass;

    qint64 m_bytesTotal = 0;
    qint64 m_bytesWritten = 0;
    FTPState m_state = Idle;
    QFile m_file;
};

#endif // FTP_PROCESS_H
