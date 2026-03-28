#include "dump_process.h"
#include "config.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstdio>

#ifndef SUBPROCESS_MODE
// ============================================================
// Main process side: DumpProcess wraps a QProcess
// ============================================================

DumpProcess::DumpProcess(const QString& drive, QObject* parent)
    : QObject(parent)
    , m_drive(drive)
{
    startSubprocess();
}

DumpProcess::~DumpProcess() {
    if (m_process) {
        m_process->kill();
        m_process->waitForFinished(2000);
        m_process->deleteLater();
    }
}

void DumpProcess::startSubprocess() {
    if (m_drive.isEmpty()) return;

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    m_process->setInputChannelMode(QProcess::ForwardedInputChannel);

    // Use absolute path to this executable
    QString exePath = QCoreApplication::applicationFilePath();
    if (exePath.isEmpty()) {
        fprintf(stderr, "DumpProcess: cannot determine executable path\n");
        return;
    }

    QStringList args;
    args << "--dump-mode" << "--drive" << m_drive;

    connect(m_process, &QProcess::readyReadStandardOutput, this, &DumpProcess::onReadyRead);
    connect(m_process, static_cast<void(QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            this, &DumpProcess::onFinished);

    m_process->start(exePath, args);
    m_started = true;
}

bool DumpProcess::isRunning() const {
    return m_process && m_process->state() == QProcess::Running;
}

void DumpProcess::sendCancel() {
    if (m_process && m_process->state() != QProcess::NotRunning) {
        QByteArray cmd = QJsonDocument(
            QJsonObject{{"type","cancel"}}
        ).toJson();
        m_process->write(cmd + "\n");
        m_process->closeWriteChannel();
    }
}

void DumpProcess::onReadyRead() {
    if (!m_process) return;
    QByteArray data = m_process->readAllStandardOutput();
    QList<QByteArray> lines = data.split('\n');
    for (const QByteArray& line : lines) {
        if (line.trimmed().isEmpty()) continue;
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(line, &err);
        if (err.error != QJsonParseError::NoError) continue;
        QJsonObject obj = doc.object();
        QString type = obj.value("type").toString();
        QString drive = obj.value("drive").toString();

        if (type == "scan_start") {
            emit scanStarted(drive);
        } else if (type == "scan_done") {
            emit scanDone(drive,
                obj.value("total_files").toInt(),
                obj.value("total_size").toVariant().toLongLong());
        } else if (type == "copy_progress") {
            emit copyProgress(drive,
                obj.value("file").toString(),
                obj.value("file_index").toInt(),
                obj.value("file_total").toInt(),
                obj.value("file_progress").toDouble(),
                obj.value("speed_mbps").toDouble(),
                obj.value("eta_seconds").toInt());
        } else if (type == "copy_file_done") {
            emit copyFileDone(drive,
                obj.value("file").toString(),
                obj.value("local_path").toString(),
                obj.value("rel_path").toString(),
                obj.value("file_size").toVariant().toLongLong(),
                obj.value("file_index").toInt(),
                obj.value("file_total").toInt());
        } else if (type == "copy_all_done") {
            emit copyAllDone(drive);
        } else if (type == "error") {
            emit error(drive, obj.value("msg").toString());
        }
    }
}

void DumpProcess::onFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    Q_UNUSED(exitCode);
    Q_UNUSED(exitStatus);
    emit finished();
}

#else
// ============================================================
// Subprocess side: actual dump logic runs here
// ============================================================

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QTimer>
#include <QSemaphore>

static QTextStream out(stdout);
static QTextStream err(stderr);

void DumpSubProcess::sendJson(const QJsonObject& obj) {
    QByteArray data = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    out << data << "\n";
    out.flush();
}

void DumpSubProcess::scanAndCopy() {
    sendJson(QJsonObject{{"type","scan_start"},{"drive",m_drive}});

    QDir srcDir(m_drive);
    if (!srcDir.exists()) {
        sendJson(QJsonObject{{"type","error"},{"drive",m_drive},{"msg","Drive does not exist"}});
        return;
    }

    // Collect video files
    QStringList nameFilters;
    for (const QString& ext : m_videoExts) {
        nameFilters << "*" + ext;
    }

    QQueue<QString> dirs;
    dirs.enqueue(m_drive);

    while (!dirs.isEmpty() && !m_cancelled) {
        QString curDir = dirs.dequeue();
        QDir d(curDir);
        if (!d.exists()) continue;

        QFileInfoList entries = d.entryInfoList(
            QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
        for (const QFileInfo& fi : entries) {
            if (fi.isDir()) {
                dirs.enqueue(fi.absoluteFilePath());
            } else if (fi.isFile()) {
                QString ext = "." + fi.suffix().toLower();
                if (m_videoExts.contains(ext)) {
                    QString srcPath = fi.absoluteFilePath();
                    QString relPath = QDir(m_drive).relativeFilePath(srcPath);
                    QString dstPath = QDir(m_localBase + "/" + m_drive).absoluteFilePath(relPath);
                    m_tasks.append(qMakePair(srcPath, dstPath));
                    m_totalSize += fi.size();
                    m_totalFiles++;
                }
            }
        }
    }

    if (m_cancelled) return;

    sendJson(QJsonObject{
        {"type","scan_done"},
        {"drive",m_drive},
        {"total_files",m_totalFiles},
        {"total_size", m_totalSize}
    });

    // Copy files
    QElapsedTimer overallTimer;
    overallTimer.start();

    for (int i = 0; i < m_tasks.size() && !m_cancelled; ++i) {
        const QString& srcPath = m_tasks[i].first;
        const QString& dstPath = m_tasks[i].second;

        QFileInfo dstInfo(dstPath);
        QDir dstDir = dstInfo.absoluteDir();
        if (!dstDir.exists()) dstDir.mkpath(".");

        QFile src(srcPath);
        if (!src.open(QIODevice::ReadOnly)) {
            sendJson(QJsonObject{
                {"type","error"},
                {"drive",m_drive},
                {"msg","Cannot open: " + srcPath}
            });
            continue;
        }

        QFile dst(dstPath);
        if (!dst.open(QIODevice::WriteOnly)) {
            sendJson(QJsonObject{
                {"type","error"},
                {"drive",m_drive},
                {"msg","Cannot create: " + dstPath}
            });
            continue;
        }

        const qint64 CHUNK = 1024 * 1024; // 1MB
        qint64 copied = 0;
        qint64 fileSize = src.size();
        char* buf = new char[CHUNK];
        QElapsedTimer fileTimer;
        fileTimer.start();
        qint64 lastReportAt = 0;

        while (!src.atEnd() && !m_cancelled) {
            qint64 r = src.read(buf, CHUNK);
            if (r <= 0) break;
            dst.write(buf, r);
            copied += r;

            // Report progress every 200ms
            if (fileTimer.elapsed() - lastReportAt > 200) {
                double filePct = fileSize > 0 ? (copied * 100.0 / fileSize) : 0;
                qint64 elapsedMs = fileTimer.elapsed();
                double speedMBps = elapsedMs > 0 ? (copied / 1024.0 / 1024.0) / (elapsedMs / 1000.0) : 0;
                // Estimate overall ETA
                double overallPct = (i + filePct / 100.0) / m_tasks.size();
                int etaSeconds = overallPct > 0 ? int((1 - overallPct) * overallTimer.elapsed() / overallPct / 1000) : 0;

                sendJson(QJsonObject{
                    {"type","copy_progress"},
                    {"drive",m_drive},
                    {"file",srcPath},
                    {"file_index",i + 1},
                    {"file_total",m_tasks.size()},
                    {"file_progress",filePct},
                    {"speed_mbps",speedMBps},
                    {"eta_seconds",etaSeconds}
                });
                lastReportAt = fileTimer.elapsed();
            }
        }

        delete[] buf;
        src.close();
        dst.close();

        sendJson(QJsonObject{
            {"type","copy_file_done"},
            {"drive",m_drive},
            {"file",srcPath},
            {"file_index",i + 1},
            {"file_total",m_tasks.size()}
        });
    }

    if (!m_cancelled) {
        sendJson(QJsonObject{{"type","copy_all_done"},{"drive",m_drive}});
    }
}

void DumpSubProcess::handleCommand(const QJsonObject& cmd) {
    QString type = cmd.value("type").toString();
    if (type == "cancel") {
        m_cancelled = true;
    }
}

// Global instance for signal handler
static DumpSubProcess* g_subprocess = nullptr;

#ifdef _WIN32
#include <windows.h>
static BOOL WINAPI consoleCtrlHandler(DWORD ctrlType) {
    Q_UNUSED(ctrlType);
    if (g_subprocess) g_subprocess->handleCommand(QJsonObject{{"type","cancel"}});
    return TRUE;
}
#endif

int dumpSubProcessMain(int argc, char* argv[]) {
    QCoreApplication a(argc, argv);

    QString drive;
    for (int i = 0; i < argc - 1; ++i) {
        if (strcmp(argv[i], "--drive") == 0) {
            drive = argv[i + 1];
        }
    }
    if (drive.isEmpty()) {
        err << "Error: --drive required" << endl;
        return 1;
    }

    Config::instance().load();
    QString localBase = Config::instance().localPath(drive);
    QStringList videoExts = Config::instance().videoExtensions();

    DumpSubProcess proc(drive, localBase, videoExts);
    g_subprocess = &proc;

#ifdef _WIN32
    SetConsoleCtrlHandler(consoleCtrlHandler, TRUE);
#endif

    // Read commands from stdin
    QSocketNotifier* notifier = new QSocketNotifier(
        fileno(stdin), QSocketNotifier::Read, &a);
    QObject::connect(notifier, &QSocketNotifier::activated, [&]() {
        char lineBuf[8192];
        if (!fgets(lineBuf, sizeof(lineBuf), stdin)) {
            // EOF
            QCoreApplication::quit();
            return;
        }
        QJsonParseError err;
        QByteArray lineBa(lineBuf);
        QJsonDocument doc = QJsonDocument::fromJson(lineBa.trimmed(), &err);
        if (err.error == QJsonParseError::NoError) {
            proc.handleCommand(doc.object());
        }
    });

    // Start scanning/copying asynchronously
    QTimer::singleShot(0, [&]() {
        proc.scanAndCopy();
        QCoreApplication::quit();
    });

    return a.exec();
}

#endif // SUBPROCESS_MODE

#include "dump_process.moc"
