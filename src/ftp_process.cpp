#include "ftp_process.h"
#include "config.h"
#include "crypto.h"
#include <QTcpSocket>
#include <QFile>
#include <QEventLoop>
#include <QFileInfo>
#include <QDir>
#include <QTimer>
#include <QCoreApplication>
#include <QRegularExpression>

FTPProcess::FTPProcess(QObject* parent)
    : QObject(parent)
    , m_cmdSocket(nullptr)
    , m_dataSocket(nullptr)
    , m_state(Idle)
    , m_currentTask(nullptr)
    , m_bytesTotal(0)
    , m_bytesWritten(0)
{
}

FTPProcess::~FTPProcess() {
    disconnectFromHost();
}

void FTPProcess::sendUpload(const FTPUploadTask& task) {
    m_queue.append(task);
    if (m_state == Idle) {
        processNext();
    }
}

void FTPProcess::sendCancelAll() {
    m_queue.clear();
    disconnectFromHost();
}

void FTPProcess::processNext() {
    if (m_queue.isEmpty()) {
        m_state = Idle;
        emit connectedChanged(false);
        return;
    }
    m_currentTask = new FTPUploadTask(m_queue.takeFirst());
    m_bytesWritten = 0;
    m_bytesTotal = m_currentTask->fileSize;
    connectToHost();
}

void FTPProcess::connectToHost() {
    Config& cfg = Config::instance();
    cfg.load();
    QJsonObject ftp = cfg.ftpConfig();

    QString host = ftp.value("host").toString();
    int port = ftp.value("port").toInt(21);
    QString user = ftp.value("username").toString();
    QString encryptedPass = ftp.value("password").toString();
    QString pass = encryptedPass;
    if (!encryptedPass.isEmpty() && cfg.isPasswordSet()) {
        pass = Crypto::decrypt(encryptedPass, cfg.encryptionPassword());
    }
    m_user = user;
    m_pass = pass;
    m_encoding = ftp.value("encoding").toString("utf8");
    if (host.isEmpty()) {
        emit uploadError(0, "FTP未配置服务器地址");
        cleanupAndNext();
        return;
    }

    if (m_cmdSocket) {
        m_cmdSocket->deleteLater();
    }
    m_cmdSocket = new QTcpSocket(this);
    connect(m_cmdSocket, &QTcpSocket::connected, this, &FTPProcess::onConnected);
    connect(m_cmdSocket, &QTcpSocket::readyRead, this, &FTPProcess::onCmdReadyRead);
    connect(m_cmdSocket, &QTcpSocket::disconnected, this, &FTPProcess::onDisconnected);
    connect(m_cmdSocket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            this, &FTPProcess::onSocketError);

    m_state = Connecting;
    emit logMessage("[FTP] 连接 " + host + ":" + QString::number(port));
    m_cmdSocket->connectToHost(host, port);
}

void FTPProcess::onConnected() {
    // Read greeting
    m_cmdSocket->readAll();
    // Send USER
    m_state = Authenticating;
    m_cmdSocket->write(QString("USER " + m_user + "\r\n").toUtf8());
}

void FTPProcess::onCmdReadyRead() {
    QByteArray resp = m_cmdSocket->readAll();
    QString response = QString::fromUtf8(resp);
    QStringList lines = response.split("\r\n", Qt::SkipEmptyParts);
    for (QString& line : lines) {
        if (line.isEmpty()) continue;
        handleResponse(line);
    }
}

void FTPProcess::handleResponse(const QString& line) {
    int code = line.left(3).toInt();
    QString msg = line.mid(4).trimmed();

    switch (m_state) {
        case Authenticating:
            if (code == 331) {
                // Need password
                m_cmdSocket->write(QString("PASS " + m_pass + "\r\n").toUtf8());
            } else if (code == 230) {
                // Anonymous login success
                m_state = Connected;
                emit connectedChanged(true);
                emit logMessage("[FTP] 登录成功");
                enterPassiveMode();
            }
            break;

        case WaitingPASV:
            if (code == 227) {
                // Parse PASV response: (h1,h2,h3,h4,p1,p2)
                int p1 = -1, p2 = -1;
                QRegularExpression rx("\\((\\d+),(\\d+),(\\d+),(\\d+),(\\d+),(\\d+)\\)");
                QRegularExpressionMatch match = rx.match(msg);
                if (match.hasMatch()) {
                    QStringList caps = match.capturedTexts();
                    m_dataHost = caps[1] + "." + caps[2] + "." + caps[3] + "." + caps[4];
                    p1 = caps[5].toInt();
                    p2 = caps[6].toInt();
                }
                if (p1 < 0) {
                    emit uploadError(m_currentTask->recordId, "PASV解析失败");
                    cleanupAndNext();
                    return;
                }
                m_dataPort = p1 * 256 + p2;
                startUpload();
            }
            break;

        case WaitingSTOR:
            if (code == 150 || code == 125) {
                // Ready to send data
                sendFileData();
            } else if (code == 226) {
                // Transfer complete
                finishUpload();
            } else {
                emit uploadError(m_currentTask->recordId, "STOR失败: " + line);
                cleanupAndNext();
            }
            break;

        case Connected:
            if (code == 230) {
                emit connectedChanged(true);
                emit logMessage("[FTP] 登录成功");
                enterPassiveMode();
            }
            break;

        case WaitingMKD:
            // 257=created, 550=already exists or error (both OK for our purpose)
            if (code == 257 || code == 550) {
                m_mkdirIndex++;
                if (m_mkdirIndex < m_mkdirQueue.size()) {
                    // Send next MKD
                    sendMkdirSync(m_mkdirQueue[m_mkdirIndex]);
                } else {
                    // All directories created, proceed to PASV
                    m_mkdirQueue.clear();
                    enterPassiveModeStep2();
                }
            } else {
                // Unexpected response, try to continue anyway
                m_mkdirIndex++;
                if (m_mkdirIndex < m_mkdirQueue.size()) {
                    sendMkdirSync(m_mkdirQueue[m_mkdirIndex]);
                } else {
                    m_mkdirQueue.clear();
                    enterPassiveModeStep2();
                }
            }
            break;

        default:
            break;
    }
}

void FTPProcess::enterPassiveMode() {
    // First ensure remote directories exist
    ensureRemoteDir(m_currentTask ? m_currentTask->remotePath : QString());
}

void FTPProcess::ensureRemoteDir(const QString& remotePath) {
    if (remotePath.isEmpty()) {
        enterPassiveModeStep2();
        return;
    }

    // Split path and create each directory component
    // remotePath like "/movie/Videos/子文件夹/子文件夹2"
    QString path = remotePath;
    if (path.startsWith("/")) path = path.mid(1);
    QStringList parts = path.split('/', Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        enterPassiveModeStep2();
        return;
    }

    // Build paths incrementally: /movie, /movie/Videos, ...
    m_mkdirQueue.clear();
    QString currentPath;
    for (int i = 0; i < parts.size() - 1; i++) {
        currentPath += "/" + parts[i];
        m_mkdirQueue.append(currentPath);
    }

    // Send first MKD command
    m_mkdirIndex = 0;
    if (!m_mkdirQueue.isEmpty()) {
        m_state = WaitingMKD;
        sendMkdirSync(m_mkdirQueue[m_mkdirIndex]);
    } else {
        enterPassiveModeStep2();
    }
}

void FTPProcess::sendMkdirSync(const QString& path) {
    if (!m_cmdSocket || m_cmdSocket->state() != QAbstractSocket::ConnectedState) return;
    m_cmdSocket->write("MKD " + encodePath(path) + "\r\n");
}

void FTPProcess::enterPassiveModeStep2() {
    m_state = WaitingPASV;
    m_cmdSocket->write("PASV\r\n");
}

void FTPProcess::startUpload() {
    if (!m_currentTask) {
        cleanupAndNext();
        return;
    }

    m_file.setFileName(m_currentTask->localPath);
    if (!QFile::exists(m_currentTask->localPath)) {
        emit uploadError(m_currentTask->recordId, "文件不存在: " + m_currentTask->localPath);
        cleanupAndNext();
        return;
    }
    if (!m_file.open(QIODevice::ReadOnly)) {
        emit uploadError(m_currentTask->recordId, "无法打开文件: " + m_currentTask->localPath);
        cleanupAndNext();
        return;
    }
    m_bytesWritten = 0;

    m_dataSocket = new QTcpSocket(this);
    connect(m_dataSocket, &QTcpSocket::connected, this, &FTPProcess::onDataConnected);
    connect(m_dataSocket, &QTcpSocket::bytesWritten, this, &FTPProcess::onDataBytesWritten);
    connect(m_dataSocket, &QTcpSocket::disconnected, this, &FTPProcess::onDataDisconnected);

    emit logMessage("[FTP] 上传: " + m_currentTask->localPath);
    m_dataSocket->connectToHost(m_dataHost, m_dataPort);
}

void FTPProcess::onDataConnected() {
    QString remotePath = m_currentTask->remotePath;
    if (!remotePath.startsWith("/")) remotePath = "/" + remotePath;
    m_state = WaitingSTOR;
    m_cmdSocket->write("STOR " + encodePath(remotePath) + "\r\n");
}

void FTPProcess::sendFileData() {
    const qint64 CHUNK = 65536;
    m_bytesWritten = 0;
    while (!m_file.atEnd()) {
        QByteArray chunk = m_file.read(CHUNK);
        if (chunk.isEmpty()) break;
        qint64 written = m_dataSocket->write(chunk);
        if (written > 0) {
            m_bytesWritten += written;
        }
        qint64 total = m_currentTask->fileSize > 0 ? m_currentTask->fileSize : m_bytesWritten;
        emit uploadProgress(m_currentTask->recordId, m_bytesWritten, total);
        // Process events to avoid blocking UI, but write immediately
        QCoreApplication::processEvents();
    }
    m_file.close();
    m_dataSocket->disconnectFromHost();
    // Note: onDataDisconnected will be called asynchronously
    // The 226 response will be handled by onCmdReadyRead
}

void FTPProcess::onDataBytesWritten(qint64 bytes) {
    // Already handled in sendFileData
}

void FTPProcess::onDataDisconnected() {
    // Just clean up the data socket - don't read m_cmdSocket here!
    // The 226 response will arrive via onCmdReadyRead
    if (m_dataSocket) {
        m_dataSocket->deleteLater();
        m_dataSocket = nullptr;
    }
}

void FTPProcess::finishUpload() {
    if (!m_currentTask) return;

    emit uploadDone(m_currentTask->recordId);
    emit logMessage("[FTP] 上传完成: " + m_currentTask->localPath);

    // Delete local file if configured
    if (Config::instance().autoDeleteLocal()) {
        QString localPath = m_currentTask->localPath;
        QFile::remove(localPath);
        emit fileDeleted(m_currentTask->recordId, localPath);
        emit logMessage("[FTP] 已删除本地文件: " + localPath);
        // Also remove empty parent directories up to local base path
        removeEmptyParentDirs(localPath);
    }

    cleanupAndNext();
}

void FTPProcess::cleanupAndNext() {
    if (m_dataSocket) {
        m_dataSocket->deleteLater();
        m_dataSocket = nullptr;
    }
    if (m_currentTask) {
        delete m_currentTask;
        m_currentTask = nullptr;
    }
    m_state = Idle;
    processNext();
}

void FTPProcess::onSocketError(QAbstractSocket::SocketError err) {
    Q_UNUSED(err);
    QString errMsg = m_cmdSocket ? m_cmdSocket->errorString() : "socket error";
    emit logMessage("[FTP] 错误: " + errMsg);
    if (m_currentTask) {
        emit uploadError(m_currentTask->recordId, errMsg);
    }
    disconnectFromHost();
    cleanupAndNext();
}

void FTPProcess::onDisconnected() {
    if (m_state != Idle) {
        emit logMessage("[FTP] 连接断开");
    }
}

void FTPProcess::disconnectFromHost() {
    if (m_cmdSocket) {
        m_cmdSocket->disconnectFromHost();
        m_cmdSocket->deleteLater();
        m_cmdSocket = nullptr;
    }
    if (m_dataSocket) {
        m_dataSocket->disconnectFromHost();
        m_dataSocket->deleteLater();
        m_dataSocket = nullptr;
    }
    m_state = Idle;
}

void FTPProcess::removeEmptyParentDirs(const QString& filePath) {
    QString localBase = QDir::cleanPath(Config::instance().localPath());
    QFileInfo fi(filePath);
    QDir parent = fi.absoluteDir();

    while (true) {
        QString parentPath = QDir::cleanPath(parent.path());
        // Stop if we've reached or passed the base path
        if (parentPath == localBase || parentPath.length() < localBase.length()) break;
        // Stop if directory doesn't exist or is not empty
        if (!parent.exists()) break;
        if (!parent.entryList(QDir::NoDotAndDotDot).isEmpty()) break;
        // Remove the empty directory
        parent.rmdir(parentPath);
        // Move up one level
        parent = QFileInfo(parentPath).absoluteDir();
    }
}

QByteArray FTPProcess::encodePath(const QString& path) {
    if (m_encoding == "gbk") {
        QTextCodec* codec = QTextCodec::codecForName("GBK");
        if (codec) {
            return codec->fromUnicode(path);
        }
    }
    // Default to UTF-8
    return path.toUtf8();
}
