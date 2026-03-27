#include "ftp_process.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QFile>
#include <QDebug>
#include <QTimer>
#include <QSemaphore>
#include <QHostAddress>
#include <QAbstractSocket>

// ============================================================
// Main process side: FTPProcess wraps QProcess
// ============================================================

FTPProcess::FTPProcess(QObject* parent)
    : QObject(parent)
{
    startSubprocess();
}

FTPProcess::~FTPProcess() {
    if (m_process) {
        m_process->kill();
        m_process->waitForFinished(2000);
        m_process->deleteLater();
    }
}

void FTPProcess::startSubprocess() {
    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    m_process->setInputChannelMode(QProcess::ForwardedInputChannel);

    QString exePath = QCoreApplication::applicationFilePath();
    m_process->start(exePath, QStringList() << "--ftp-mode");

    m_started = true;
    connect(m_process, &QProcess::readyReadStandardOutput, this, &FTPProcess::onReadyRead);
    connect(m_process, &QProcess::finished, this, &FTPProcess::onFinished);
}

void FTPProcess::sendUpload(const FTPUploadTask& task) {
    if (!m_process || m_process->state() == QProcess::NotRunning) return;
    QMutexLocker l(&m_mutex);
    QJsonObject obj;
    obj["type"] = "upload";
    obj["record_id"] = task.recordId;
    obj["local_path"] = task.localPath;
    obj["remote_path"] = task.remotePath;
    obj["file_size"] = task.fileSize;
    QByteArray data = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    m_process->write(data + "\n");
}

void FTPProcess::sendCancelAll() {
    if (!m_process || m_process->state() == QProcess::NotRunning) return;
    QByteArray cmd = QJsonDocument(QJsonObject{{"type","cancel"}}).toJson();
    m_process->write(cmd + "\n");
}

void FTPProcess::onReadyRead() {
    if (!m_process) return;
    QByteArray data = m_process->readAllStandardOutput();
    QList<QByteArray> lines = data.split('\n');
    for (const QByteArray& line : lines) {
        if (line.trimmed().isEmpty()) continue;
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(line, &err);
        if (err.error != QJsonParseError::NoError) continue;
        handleJson(doc.object());
    }
}

void FTPProcess::handleJson(const QJsonObject& obj) {
    QString type = obj.value("type").toString();

    if (type == "ftp_upload_progress") {
        emit uploadProgress(
            obj.value("record_id").toInt(),
            obj.value("uploaded_bytes").toVariant().toLongLong(),
            obj.value("total_bytes").toVariant().toLongLong());
    } else if (type == "ftp_upload_done") {
        emit uploadDone(obj.value("record_id").toInt());
    } else if (type == "ftp_file_deleted") {
        emit fileDeleted(obj.value("record_id").toInt());
    } else if (type == "ftp_error") {
        emit uploadError(
            obj.value("record_id").toInt(),
            obj.value("msg").toString());
    } else if (type == "ftp_connected") {
        m_connected = true;
        emit connectedChanged(true);
        emit logMessage("[FTP] 子进程已连接");
    } else if (type == "ftp_disconnected") {
        m_connected = false;
        emit connectedChanged(false);
    } else if (type == "log") {
        emit logMessage(obj.value("msg").toString());
    }
}

void FTPProcess::onFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    Q_UNUSED(exitCode);
    Q_UNUSED(exitStatus);
    m_started = false;
    m_connected = false;
    emit connectedChanged(false);
    // Restart subprocess after a delay
    QTimer::singleShot(1000, this, [this]() {
        if (m_started) return;
        startSubprocess();
    });
}

// ============================================================
// Subprocess side: raw socket FTP implementation
// ============================================================
// Compiled with -DFTP_SUBPROCESS_MODE
// (this is in the same file, selected by the --ftp-mode flag)

#ifdef FTP_SUBPROCESS_MODE

#include <QCoreApplication>
#include <QTcpSocket>
#include <QTimer>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QHostAddress>
#include <QAbstractSocket>

static QTextStream out(stdout);

static void sendJson(QTextStream& os, const QJsonObject& obj) {
    os << QJsonDocument(obj).toJson(QJsonDocument::Compact) << "\n";
    os.flush();
}

class FTPSubClient : public QObject {
    Q_OBJECT
public:
    FTPSubClient(const FTPUploadTask& task, QTcpSocket* cmdSocket,
                 const QString& host, int port,
                 const QString& user, const QString& pass,
                 bool useTls, QObject* parent = nullptr)
        : QObject(parent)
        , m_task(task)
        , m_cmdSocket(cmdSocket)
        , m_host(host)
        , m_port(port)
        , m_user(user)
        , m_pass(pass)
        , m_useTls(useTls)
        , m_dataSocket(nullptr)
    {
        connect(m_cmdSocket, &QTcpSocket::readyRead, this, &FTPSubClient::onCmdReadyRead);
        connect(m_cmdSocket, &QTcpSocket::disconnected, this, &FTPSubClient::onDisconnected);
        connect(m_cmdSocket, &QTcpSocket::error, this, &FTPSubClient::onError);
        sendJson(out, QJsonObject{{"type","log"},{"msg",QString("[FTP] 开始上传: %1").arg(task.localPath)}});
        connectToHost();
    }

private slots:
    void connectToHost() {
        m_cmdSocket->connectToHost(m_host, m_port);
        if (!m_cmdSocket->waitForConnected(10000)) {
            emitError("连接服务器失败: " + m_cmdSocket->errorString());
            return;
        }
        // Read greeting
        readResponse();
    }

    void onCmdReadyRead() {
        m_cmdBuf += QString::fromUtf8(m_cmdSocket->readAll());
        processResponses();
    }

    void onDisconnected() {
        cleanup();
    }

    void onError(QAbstractSocket::SocketError) {
        emitError("Socket错误: " + m_cmdSocket->errorString());
    }

    void onDataReady() {
        if (!m_dataSocket || !m_file) return;
        QByteArray data = m_dataSocket->readAll();
        m_file->write(data);
        m_uploadedBytes += data.size();
        m_totalWritten += data.size();

        // Emit progress
        sendJson(out, QJsonObject{
            {"type","ftp_upload_progress"},
            {"record_id",m_task.recordId},
            {"uploaded_bytes",m_uploadedBytes},
            {"total_bytes",m_task.fileSize}
        });
    }

    void onDataDisconnected() {
        if (m_file) {
            m_file->close();
            delete m_file;
            m_file = nullptr;
        }
        if (m_dataSocket) {
            m_dataSocket->deleteLater();
            m_dataSocket = nullptr;
        }
        // Send completion
        sendJson(out, QJsonObject{
            {"type","ftp_upload_done"},
            {"record_id",m_task.recordId}
        });
        cleanup();
    }

    void onDataError(QAbstractSocket::SocketError) {
        emitError("数据连接错误: " + m_dataSocket->errorString());
    }

private:
    void processResponses() {
        QStringList lines = m_cmdBuf.split("\r\n", Qt::SkipEmptyParts);
        for (const QString& line : lines) {
            if (line.length() < 4) continue;
            QString code = line.left(3);
            QString msg = line.mid(4);
            handleResponse(code, msg);
        }
        m_cmdBuf.clear();
    }

    void readResponse() {
        // Will be called when readyRead fires
    }

    void handleResponse(const QString& code, const QString& msg) {
        Q_UNUSED(msg);
        if (code == "220") {
            // Send USER
            m_cmdSocket->write(QString("USER %1\r\n").arg(m_user).toUtf8());
        } else if (code == "331") {
            // Send PASS
            m_cmdSocket->write(QString("PASS %1\r\n").arg(m_pass).toUtf8());
        } else if (code == "230") {
            // Login success
            sendJson(out, QJsonObject{{"type","log"},{"msg","[FTP] 登录成功"}});
            // Set passive mode
            m_cmdSocket->write("PASV\r\n");
        } else if (code == "227") {
            // Parse PASV response: (h1,h2,h3,h4,p1,p2)
            QRegExp rx("\\(([0-9,]+)\\)");
            if (rx.indexIn(msg) >= 0) {
                QStringList parts = rx.cap(1).split(",");
                if (parts.size() == 6) {
                    QString host = QString("%1.%2.%3.%4")
                        .arg(parts[0]).arg(parts[1]).arg(parts[2]).arg(parts[3]);
                    int port = parts[4].toInt() * 256 + parts[5].toInt();
                    m_dataHost = host;
                    m_dataPort = port;
                    // Open data connection
                    openDataConnection();
                }
            }
        } else if (code == "150" || code == "125") {
            // Data connection opened / Transfer starting
            // Data socket will notify on readyRead
        } else if (code == "226") {
            // Transfer complete
            sendJson(out, QJsonObject{
                {"type","ftp_upload_done"},
                {"record_id",m_task.recordId}
            });
            // Delete local file after upload
            if (QFile::exists(m_task.localPath)) {
                QFile::remove(m_task.localPath);
                sendJson(out, QJsonObject{
                    {"type","ftp_file_deleted"},
                    {"record_id",m_task.recordId}
                });
            }
            m_cmdSocket->write("QUIT\r\n");
            cleanup();
        } else if (code == "530") {
            emitError("登录失败: " + msg);
        } else if (code == "550") {
            emitError("文件操作失败: " + msg);
        }
    }

    void openDataConnection() {
        m_dataSocket = new QTcpSocket(this);
        connect(m_dataSocket, &QTcpSocket::readyRead, this, &FTPSubClient::onDataReady);
        connect(m_dataSocket, &QTcpSocket::disconnected, this, &FTPSubClient::onDataDisconnected);
        connect(m_dataSocket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
                this, &FTPSubClient::onDataError);
        m_dataSocket->connectToHost(m_dataHost, m_dataPort);

        // Open local file
        m_file = new QFile(m_task.localPath);
        if (!m_file->open(QIODevice::ReadOnly)) {
            emitError("Cannot open local file: " + m_task.localPath);
            return;
        }
        m_uploadedBytes = 0;
        m_totalWritten = 0;

        // Send STOR command
        QString filename = QFileInfo(m_task.localPath).fileName();
        QString remotePath = m_task.remotePath;
        if (!remotePath.startsWith("/")) remotePath = "/" + remotePath;
        m_cmdSocket->write(QString("STOR %1\r\n").arg(remotePath).toUtf8());

        // Start reading data after small delay
        QTimer::singleShot(100, this, &FTPSubClient::sendFileData);
    }

    void sendFileData() {
        if (!m_file || m_file->atEnd()) return;
        if (!m_dataSocket || m_dataSocket->state() != QAbstractSocket::ConnectedState) return;

        QByteArray chunk = m_file->read(65536);
        if (!chunk.isEmpty()) {
            m_dataSocket->write(chunk);
            m_uploadedBytes += chunk.size();
            sendJson(out, QJsonObject{
                {"type","ftp_upload_progress"},
                {"record_id",m_task.recordId},
                {"uploaded_bytes",m_uploadedBytes},
                {"total_bytes",m_task.fileSize}
            });
        }

        if (!m_file->atEnd()) {
            QTimer::singleShot(0, this, &FTPSubClient::sendFileData);
        }
    }

    void emitError(const QString& errMsg) {
        sendJson(out, QJsonObject{
            {"type","ftp_error"},
            {"record_id",m_task.recordId},
            {"msg",errMsg}
        });
        cleanup();
    }

    void cleanup() {
        if (m_file) {
            m_file->close();
            delete m_file;
            m_file = nullptr;
        }
        if (m_dataSocket) {
            m_dataSocket->disconnect();
            m_dataSocket->deleteLater();
            m_dataSocket = nullptr;
        }
        if (m_cmdSocket) {
            m_cmdSocket->disconnect();
            m_cmdSocket->deleteLater();
            m_cmdSocket = nullptr;
        }
        deleteLater();
    }

    FTPUploadTask m_task;
    QTcpSocket* m_cmdSocket = nullptr;
    QString m_host;
    int m_port = 21;
    QString m_user;
    QString m_pass;
    bool m_useTls = false;
    QTcpSocket* m_dataSocket = nullptr;
    QString m_dataHost;
    int m_dataPort = 0;
    QFile* m_file = nullptr;
    qint64 m_uploadedBytes = 0;
    qint64 m_totalWritten = 0;
    QString m_cmdBuf;
};

class FTPSubProcess : public QObject {
    Q_OBJECT
public:
    FTPSubProcess(QObject* parent = nullptr) : QObject(parent) {}

    void handleCommand(const QJsonObject& cmd) {
        QString type = cmd.value("type").toString();
        if (type == "upload") {
            FTPUploadTask task;
            task.recordId = cmd.value("record_id").toInt();
            task.localPath = cmd.value("local_path").toString();
            task.remotePath = cmd.value("remote_path").toString();
            task.fileSize = cmd.value("file_size").toVariant().toLongLong();
            task.status = "pending";
            startUpload(task);
        } else if (type == "cancel") {
            for (FTPSubClient* client : m_clients) {
                client->deleteLater();
            }
            m_clients.clear();
        }
    }

private slots:
    void startUpload(const FTPUploadTask& task) {
        // Load FTP config
        Config::instance().load();
        QJsonObject ftp = Config::instance().ftpConfig();
        QString host = ftp.value("host").toString();
        int port = ftp.value("port").toInt(21);
        QString user = ftp.value("username").toString();
        QString encryptedPass = ftp.value("password").toString();
        QString pass = encryptedPass;
        if (!encryptedPass.isEmpty() && Config::instance().isPasswordSet()) {
            pass = Crypto::decrypt(encryptedPass, Config::instance().encryptionPassword());
        }
        bool useTls = ftp.value("use_tls").toBool();

        if (host.isEmpty()) {
            sendJson(out, QJsonObject{
                {"type","ftp_error"},
                {"record_id",task.recordId},
                {"msg","FTP服务器未配置"}
            });
            return;
        }

        QTcpSocket* cmdSocket = new QTcpSocket(this);
        FTPSubClient* client = new FTPSubClient(task, cmdSocket, host, port,
                                                user, pass, useTls, this);
        m_clients.append(client);
        connect(client, &QObject::destroyed, this, [this, client]() {
            m_clients.removeAll(client);
        });
    }

private:
    QList<FTPSubClient*> m_clients;
};

int ftpSubProcessMain(int argc, char* argv[]) {
    QCoreApplication a(argc, argv);

    FTPSubProcess proc;
    sendJson(out, QJsonObject{{"type","ftp_connected"}});

    // Read commands from stdin
    QTextStream in(stdin);
    QSocketNotifier notifier(fileno(stdin), QSocketNotifier::Read, &a);
    QObject::connect(&notifier, &QSocketNotifier::activated, [&]() {
        QString line = in.readLine();
        if (line.isNull() || line.isEmpty()) {
            sendJson(out, QJsonObject{{"type","ftp_disconnected"}});
            QCoreApplication::quit();
            return;
        }
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(line.trimmed(), &err);
        if (err.error == QJsonParseError::NoError) {
            proc.handleCommand(doc.object());
        }
    });

    return a.exec();
}

#endif // FTP_SUBPROCESS_MODE
