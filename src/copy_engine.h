#ifndef COPY_ENGINE_H
#define COPY_ENGINE_H

#include <QObject>
#include <QString>
#include <QList>
#include <QMutex>
#include <QThread>
#include <QMap>

struct CopyTask {
    QString usbDrive;
    QString srcPath;
    QString dstPath;
    qint64 fileSize = 0;
    int recordId = 0;
    double progress = 0;
    QString status; // pending/copying/copied/error
    QString errorMsg;
};

class CopyEngine : public QObject {
    Q_OBJECT
public:
    explicit CopyEngine(QObject* parent = nullptr);
    ~CopyEngine();

    void startCopy(const QString& drive);
    void cancelCopy(const QString& drive);
    bool isCopying(const QString& drive) const;

signals:
    void copyProgress(const QString& drive, int done, int total, const CopyTask& task);
    void copyDone(const QString& drive, const QList<CopyTask>& tasks);

private:
    void copyWorker(const QString& drive);

    QMap<QString, QList<CopyTask>> m_tasks;
    QMap<QString, bool> m_cancel;
    QMap<QString, QThread*> m_threads;
    QMutex m_mutex;
};

#endif // COPY_ENGINE_H
