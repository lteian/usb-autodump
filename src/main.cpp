#include <QApplication>
#include <QMessageBox>
#include <QDebug>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTcpSocket>
#include <QSocketNotifier>
#include <QQueue>
#include <QTextStream>
#include <QRegExp>
#include <QStandardPaths>
#include <QVariant>
#include <QSharedMemory>
#include <QLocalServer>
#include <QDate>

// Max log file size: 5MB
static const qint64 MAX_LOG_SIZE = 5 * 1024 * 1024;

// Get daily log file path: {appDir}/logs/{yyyy-MM-dd}.log
static QString getDailyLogPath() {
    QString appDir = QCoreApplication::applicationDirPath();
    QString logDir = appDir + "/logs";
    QDir().mkpath(logDir);
    QString dateStr = QDate::currentDate().toString("yyyy-MM-dd");
    return logDir + "/" + dateStr + ".log";
}

// Write important message to daily log file (truncates if > 5MB)
static void writeImportantLog(const QString& msg) {
    QString path = getDailyLogPath();
    QFile f(path);
    // Truncate if file exceeds max size
    if (f.size() > MAX_LOG_SIZE) {
        f.open(QIODevice::WriteOnly | QIODevice::Text);
        f.write(QString("==== Log truncated (size limit exceeded) ====\n").toUtf8());
        f.close();
    }
    if (f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream ts(&f);
        ts << QDateTime::currentDateTime().toString("hh:mm:ss") << " " << msg << "\n";
    }
}
#include <QLocalSocket>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "mainwindow.h"
#include "config.h"
#include "crypto.h"

// Ensure only one instance runs as main process
// Subprocesses (--dump-mode / --ftp-mode) bypass this check
static bool s_isSubprocess = false;

static bool checkSingleInstance() {
    // Try to connect to an existing server
    QLocalSocket socket;
    socket.connectToServer("USB-AutoDumper-Main");
    if (socket.waitForConnected(500)) {
        // Another instance is already running as main process
        return false;
    }

    // No existing instance - we become the server
    QLocalServer server;
    server.setSocketOptions(QLocalServer::UserAccessOption);
    if (!server.listen("USB-AutoDumper-Main")) {
        // Maybe another process started at the same time - try connect again
        socket.connectToServer("USB-AutoDumper-Main");
        if (socket.waitForConnected(500)) return false;
        // Could not bind, assume we're subprocess
        return false;
    }
    return true;
}

int main(int argc, char *argv[]) {
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication a(argc, argv);
    a.setApplicationName("USB自动转储工具");
    a.setOrganizationName("usb-autodump");

    // Install message handler to write debug output to daily log file
    writeImportantLog("==== Application started ====");
    qInstallMessageHandler([](QtMsgType, const QMessageLogContext &, const QString &msg) {
        writeImportantLog(msg);
    });

    // Check for subprocess modes FIRST
    bool dumpMode = false;
    bool ftpMode = false;
    QString drive;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--dump-mode") == 0) {
            dumpMode = true;
            s_isSubprocess = true;
        }
        if (strcmp(argv[i], "--drive") == 0 && i + 1 < argc) {
            drive = argv[i + 1];
        }
        if (strcmp(argv[i], "--ftp-mode") == 0) {
            ftpMode = true;
            s_isSubprocess = true;
        }
    }

    // Only main process (non-subprocess) checks single instance
    // This prevents the main app from starting multiple times
    if (!s_isSubprocess) {
        if (!checkSingleInstance()) {
            // Another instance is already running - just exit silently
            return 0;
        }
    }

    // Ensure config exists
    Config::instance().load();

    if (dumpMode) {
        // Run as dump subprocess
        // Use DumpSubProcess directly via the dump_process header
        // Note: we use the same executable, checking for dump mode
        // The subprocess entry point is handled in dump_process.cpp
        // We just need to signal readiness and run
        // Since we can't easily call a nested main, we inline the logic here

        if (drive.isEmpty()) {
            fprintf(stderr, "Error: --drive required\n");
            return 1;
        }

        Config::instance().load();
        QString localBase = Config::instance().localPath(drive);
        QStringList videoExts = Config::instance().videoExtensions();

        // Run dump in the same process (since it's already a subprocess)
        // Just import and run
        // The actual subprocess code is in dump_process.cpp SUBPROCESS_MODE section
        // For simplicity, run the same process as a subprocess
        // We use a signal to indicate readiness
        printf("READY\n");
        fflush(stdout);

        // Actually, let's just use the dump subprocess inline
        // Run the dump loop inline as a subprocess
        class DumpRunner {
        public:
            DumpRunner(const QString& drive, const QString& localBase, const QStringList& videoExts)
                : m_drive(drive), m_localBase(localBase), m_videoExts(videoExts) {}

            void run() {
                auto sendJson = [](const QJsonObject& obj) {
                    QByteArray d = QJsonDocument(obj).toJson(QJsonDocument::Compact);
                    printf("%s\n", d.constData());
                    fflush(stdout);
                };

                sendJson(QJsonObject{{"type","scan_start"},{"drive",m_drive}});

                // Collect video files
                QDir srcDir(m_drive);
                if (!srcDir.exists()) {
                    sendJson(QJsonObject{{"type","error"},{"drive",m_drive},{"msg","Drive does not exist"}});
                    return;
                }

                QList<QPair<QString,QString>> tasks; // (srcPath, relPath)
                qint64 totalSize = 0;
                int totalFiles = 0;

                QQueue<QString> dirs;
                dirs.enqueue(m_drive);

                while (!dirs.isEmpty()) {
                    QString curDir = dirs.dequeue();
                    QDir d(curDir);
                    if (!d.exists()) continue;
                    QFileInfoList entries = d.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
                    for (const QFileInfo& fi : entries) {
                        if (fi.isDir()) {
                            dirs.enqueue(fi.absoluteFilePath());
                        } else if (fi.isFile()) {
                            QString ext = "." + fi.suffix().toLower();
                            if (m_videoExts.contains(ext)) {
                                QString srcPath = fi.absoluteFilePath();
                                QString relPath = QDir(m_drive).relativeFilePath(srcPath);
                                // relPath is like "子文件夹/video.mp4" (no drive letter)
                                QString dstPath = QDir(m_localBase).filePath(relPath);
                                tasks.append(qMakePair(srcPath, relPath));
                                totalSize += fi.size();
                                totalFiles++;
                            }
                        }
                    }
                }

                sendJson(QJsonObject{
                    {"type","scan_done"},
                    {"drive",m_drive},
                    {"total_files",totalFiles},
                    {"total_size", totalSize}
                });

                // Copy files
                for (int i = 0; i < tasks.size(); ++i) {
                    const QString& srcPath = tasks[i].first;
                    const QString& relPath = tasks[i].second;
                    const QString dstPath = QDir(m_localBase).filePath(relPath);

                    QFileInfo dstInfo(dstPath);
                    QDir dstDir = dstInfo.absoluteDir();
                    if (!dstDir.exists()) dstDir.mkpath(".");

                    QFile src(srcPath);
                    if (!src.open(QIODevice::ReadOnly)) {
                        sendJson(QJsonObject{{"type","error"},{"drive",m_drive},{"msg","Cannot open: " + srcPath}});
                        continue;
                    }

                    QFile dst(dstPath);
                    if (!dst.open(QIODevice::WriteOnly)) {
                        sendJson(QJsonObject{{"type","error"},{"drive",m_drive},{"msg","Cannot create: " + dstPath}});
                        continue;
                    }

                    const qint64 CHUNK = 1024 * 1024;
                    qint64 copied = 0;
                    qint64 fileSize = src.size();
                    QByteArray buf;
                    buf.reserve(CHUNK);
                    QElapsedTimer fileTimer;
                    fileTimer.start();

                    while (!src.atEnd()) {
                        buf = src.read(CHUNK);
                        if (buf.isEmpty()) break;
                        dst.write(buf);
                        copied += buf.size();
                        double filePct = fileSize > 0 ? (copied * 100.0 / fileSize) : 0;
                        qint64 elapsedMs = fileTimer.elapsed();
                        double speedMBps = elapsedMs > 0 ? (copied / 1024.0 / 1024.0) / (elapsedMs / 1000.0) : 0;
                        sendJson(QJsonObject{
                            {"type","copy_progress"},
                            {"drive",m_drive},
                            {"file",srcPath},
                            {"file_index",i+1},
                            {"file_total",tasks.size()},
                            {"file_progress",filePct},
                            {"speed_mbps",speedMBps},
                            {"eta_seconds",0}
                        });
                    }
                    src.close();
                    dst.close();

                    sendJson(QJsonObject{
                        {"type","copy_file_done"},
                        {"drive",m_drive},
                        {"file",srcPath},
                        {"local_path",dstPath},
                        {"rel_path",relPath},
                        {"file_size",fileSize},
                        {"file_index",i+1},
                        {"file_total",tasks.size()}
                    });
                }

                sendJson(QJsonObject{{"type","copy_all_done"},{"drive",m_drive}});
            }

        private:
            QString m_drive, m_localBase;
            QStringList m_videoExts;
        };

        DumpRunner runner(drive, localBase, videoExts);
        runner.run();
        return 0;
    }

    if (ftpMode) {
        // FTP subprocess mode - handled in ftp_process.cpp
        // Just run the main event loop here
        // The actual FTP subprocess logic is in ftp_process.cpp
        // We call the ftpSubProcessMain inline

        class FTPSubRunner {
        public:
            FTPSubRunner() {}

            void run() {
                // Load config
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

                auto sendJson = [](const QJsonObject& obj) {
                    QByteArray d = QJsonDocument(obj).toJson(QJsonDocument::Compact);
                    printf("%s\n", d.constData());
                    fflush(stdout);
                };

                sendJson(QJsonObject{{"type","ftp_connected"}});

                // Simple event loop for FTP subprocess
                // Read from stdin, write to stdout
                // The actual FTP logic runs in this event loop
                QSocketNotifier notifier(fileno(stdin), QSocketNotifier::Read, nullptr);
                QObject::connect(&notifier, &QSocketNotifier::activated, [&]() {
                    char line[8192];
                    if (fgets(line, sizeof(line), stdin)) {
                        QString s = QString::fromUtf8(line).trimmed();
                        if (s.isEmpty()) {
                            sendJson(QJsonObject{{"type","ftp_disconnected"}});
                            exit(0);
                            return;
                        }
                        QJsonParseError err;
                        QJsonDocument doc = QJsonDocument::fromJson(s.toUtf8(), &err);
                        if (err.error != QJsonParseError::NoError) return;
                        QJsonObject cmd = doc.object();
                        QString type = cmd.value("type").toString();

                        if (type == "upload") {
                            int recordId = cmd.value("record_id").toInt();
                            QString localPath = cmd.value("local_path").toString();
                            QString remotePath = cmd.value("remote_path").toString();
                            qint64 fileSize = cmd.value("file_size").toVariant().toLongLong();

                            sendJson(QJsonObject{{"type","log"},{"msg",QString("[FTP] 上传: %1").arg(localPath)}});

                            // Use raw socket FTP
                            QTcpSocket cmdSocket;
                            cmdSocket.connectToHost(host, port);
                            if (!cmdSocket.waitForConnected(10000)) {
                                sendJson(QJsonObject{{"type","ftp_error"},{"record_id",recordId},{"msg","连接失败: " + cmdSocket.errorString()}});
                                return;
                            }

                            // Read greeting
                            cmdSocket.readAll();

                            // Send USER
                            cmdSocket.write(QString("USER %1\r\n").arg(user).toUtf8());
                            cmdSocket.waitForReadyRead(5000);
                            QString resp = QString::fromUtf8(cmdSocket.readAll());
                            if (!resp.startsWith("331")) {
                                sendJson(QJsonObject{{"type","ftp_error"},{"record_id",recordId},{"msg","USER失败: " + resp}});
                                return;
                            }

                            // Send PASS
                            cmdSocket.write(QString("PASS %1\r\n").arg(pass).toUtf8());
                            cmdSocket.waitForReadyRead(5000);
                            resp = QString::fromUtf8(cmdSocket.readAll());
                            if (!resp.startsWith("230")) {
                                sendJson(QJsonObject{{"type","ftp_error"},{"record_id",recordId},{"msg","PASS失败: " + resp}});
                                return;
                            }
                            sendJson(QJsonObject{{"type","log"},{"msg","[FTP] 登录成功"}});

                            // PASV
                            cmdSocket.write("PASV\r\n");
                            cmdSocket.waitForReadyRead(5000);
                            resp = QString::fromUtf8(cmdSocket.readAll());

                            // Parse PASV
                            QRegExp rx("\\(([0-9,]+)\\)");
                            if (rx.indexIn(resp) < 0) {
                                sendJson(QJsonObject{{"type","ftp_error"},{"record_id",recordId},{"msg","PASV解析失败"}});
                                return;
                            }
                            QStringList parts = rx.cap(1).split(",");
                            QString dataHost = QString("%1.%2.%3.%4").arg(parts[0]).arg(parts[1]).arg(parts[2]).arg(parts[3]);
                            int dataPort = parts[4].toInt() * 256 + parts[5].toInt();

                            // Open data connection
                            QTcpSocket dataSocket;
                            dataSocket.connectToHost(dataHost, dataPort);
                            dataSocket.waitForConnected(10000);

                            // STOR
                            if (!remotePath.startsWith("/")) remotePath = "/" + remotePath;
                            cmdSocket.write(QString("STOR %1\r\n").arg(remotePath).toUtf8());
                            cmdSocket.waitForReadyRead(5000);
                            resp = QString::fromUtf8(cmdSocket.readAll());

                            // Upload file
                            QFile file(localPath);
                            if (file.open(QIODevice::ReadOnly)) {
                                qint64 totalWritten = 0;
                                while (!file.atEnd()) {
                                    QByteArray chunk = file.read(65536);
                                    if (chunk.isEmpty()) break;
                                    dataSocket.write(chunk);
                                    totalWritten += chunk.size();
                                    sendJson(QJsonObject{
                                        {"type","ftp_upload_progress"},
                                        {"record_id",recordId},
                                        {"uploaded_bytes",totalWritten},
                                        {"total_bytes",fileSize}
                                    });
                                    dataSocket.waitForBytesWritten(100);
                                }
                                file.close();
                                dataSocket.waitForDisconnected(5000);
                            }
                            cmdSocket.readAll(); // 226
                            cmdSocket.write("QUIT\r\n");
                            cmdSocket.disconnectFromHost();

                            sendJson(QJsonObject{{"type","ftp_upload_done"},{"record_id",recordId}});

                            // Delete local file if configured
                            if (Config::instance().autoDeleteLocal()) {
                                QFile::remove(localPath);
                                sendJson(QJsonObject{{"type","ftp_file_deleted"},{"record_id",recordId}});
                            }
                        } else if (type == "cancel") {
                            sendJson(QJsonObject{{"type","log"},{"msg","[FTP] 取消所有上传"}});
                        }
                    }
                });

                return;
            }
        };

        FTPSubRunner runner;
        runner.run();
        return a.exec();
    }

    // Normal main process - show main window
    MainWindow w;
    w.show();
    return a.exec();
}
