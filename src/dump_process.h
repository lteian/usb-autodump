#ifndef DUMP_PROCESS_H
#define DUMP_PROCESS_H

// dump_process.h - Header for dump subprocess (used by both main process launcher and subprocess itself)

#ifndef SUBPROCESS_MODE
// When compiled as part of main app (launcher side)
#include <QObject>
#include <QString>
#include <QProcess>
#include <QMap>

class DumpProcess : public QObject {
    Q_OBJECT
public:
    explicit DumpProcess(const QString& drive, QObject* parent = nullptr);
    ~DumpProcess();

    QString drive() const { return m_drive; }
    bool isRunning() const;

    // Send commands to subprocess
    void sendCancel();

signals:
    void scanStarted(const QString& drive);
    void scanDone(const QString& drive, int totalFiles, qint64 totalSize);
    void copyProgress(const QString& drive, const QString& file,
                      int fileIndex, int fileTotal,
                      double fileProgress, double speedMBps, int etaSeconds);
    void copyFileDone(const QString& drive, const QString& file,
                     int fileIndex, int fileTotal);
    void copyAllDone(const QString& drive);
    void error(const QString& drive, const QString& msg);
    void finished();

private slots:
    void onReadyRead();
    void onFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    void startSubprocess();
    QString m_drive;
    QProcess* m_process = nullptr;
    bool m_started = false;
};

#else
// SUBPROCESS_MODE - actual subprocess implementation
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQueue>
#include <QSocketNotifier>
#include <QThread>
#include <QTimer>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#endif

class DumpSubProcess {
public:
    DumpSubProcess(const QString& drive, const QString& localPath, const QStringList& videoExts);

    void run();
    void handleCommand(const QJsonObject& cmd);

    // These are called from the subprocess
    void sendJson(const QJsonObject& obj);
    void scanAndCopy();

private:
    QString m_drive;
    QString m_localBase;
    QStringList m_videoExts;
    QList<QPair<QString, QString>> m_tasks; // src -> dst
    volatile bool m_cancelled = false;
    qint64 m_totalSize = 0;
    int m_totalFiles = 0;
};

#endif // SUBPROCESS_MODE

#endif // DUMP_PROCESS_H
