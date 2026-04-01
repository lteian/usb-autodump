#include "ftp_pool.h"

FTPConnectionPool::FTPConnectionPool(QObject* parent, int maxConnections)
    : QObject(parent)
    , m_maxConnections(maxConnections)
    , m_workerIndex(0)
{
    for (int i = 0; i < maxConnections; ++i) {
        FTPProcess* worker = new FTPProcess(this);
        m_workers.append(worker);

        connect(worker, &FTPProcess::uploadProgress, this, &FTPConnectionPool::onUploadProgress);
        connect(worker, &FTPProcess::uploadDone, this, &FTPConnectionPool::onUploadDone);
        connect(worker, &FTPProcess::uploadError, this, &FTPConnectionPool::onUploadError);
        connect(worker, &FTPProcess::connectedChanged, this, &FTPConnectionPool::onConnectedChanged);
        connect(worker, &FTPProcess::fileDeleted, this, &FTPConnectionPool::onFileDeleted);
        connect(worker, &FTPProcess::logMessage, this, &FTPConnectionPool::onLogMessage);
    }
}

FTPConnectionPool::~FTPConnectionPool() {
    for (FTPProcess* worker : m_workers) {
        worker->deleteLater();
    }
}

void FTPConnectionPool::sendUpload(const FTPUploadTask& task) {
    QMutexLocker locker(&m_mutex);
    m_pendingQueue.enqueue(task);
    assignTaskToWorker();
}

void FTPConnectionPool::sendCancelAll() {
    QMutexLocker locker(&m_mutex);
    m_pendingQueue.clear();
    for (FTPProcess* worker : m_workers) {
        worker->sendCancelAll();
    }
}

void FTPConnectionPool::assignTaskToWorker() {
    // Distribute pending tasks evenly across workers
    // Each worker processes sequentially, but we spread tasks across all workers
    for (int i = 0; i < m_workers.size() && !m_pendingQueue.isEmpty(); ++i) {
        int idx = (m_workerIndex + i) % m_workers.size();
        FTPProcess* worker = m_workers[idx];
        FTPUploadTask task = m_pendingQueue.dequeue();
        worker->sendUpload(task);
    }
    m_workerIndex = (m_workerIndex + 1) % m_workers.size();
}

void FTPConnectionPool::onUploadDone(int recordId) {
    emit uploadDone(recordId);
}

void FTPConnectionPool::onUploadError(int recordId, const QString& msg) {
    emit uploadError(recordId, msg);
}

void FTPConnectionPool::onUploadProgress(int recordId, qint64 uploadedBytes, qint64 totalBytes) {
    emit uploadProgress(recordId, uploadedBytes, totalBytes);
}

void FTPConnectionPool::onConnectedChanged(bool connected) {
    emit connectedChanged(connected);
}

void FTPConnectionPool::onFileDeleted(int recordId, const QString& localPath) {
    emit fileDeleted(recordId, localPath);
}

void FTPConnectionPool::onLogMessage(const QString& msg) {
    emit logMessage(msg);
}