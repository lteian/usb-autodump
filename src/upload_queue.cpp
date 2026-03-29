#include "upload_queue.h"
#include <QVBoxLayout>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QHeaderView>
#include <QFileInfo>

UploadQueue::UploadQueue(QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    m_tree = new QTreeWidget();
    m_tree->setHeaderLabels({"名称", "大小", "状态"});
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tree->setAlternatingRowColors(false);
    m_tree->setRootIsDecorated(true);
    m_tree->setIndentation(16);
    m_tree->setStyleSheet(R"(
        QTreeWidget {
            background: transparent;
            color: #1E293B;
            border: none;
            font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif;
            font-size: 12px;
        }
        QTreeWidget::item {
            padding: 2px 0;
        }
        QTreeWidget::item:selected {
            background: transparent;
        }
        QTreeWidget::item:hover {
            background: transparent;
        }
        QHeaderView::section {
            background: transparent;
            color: #94A3B8;
            border: none;
            border-bottom: 1px solid #E2E8F0;
            padding: 3px 0;
            font-size: 11px;
            font-weight: normal;
        }
    )");
    vl->addWidget(m_tree);
}

QString formatSize(qint64 b) {
    if (b >= 1024*1024*1024) return QString::number(b/1024.0/1024/1024,'f',2)+"GB";
    if (b >= 1024*1024) return QString::number(b/1024.0/1024,'f',2)+"MB";
    if (b >= 1024) return QString::number(b/1024.0,'f',1)+"KB";
    return QString::number(b)+"B";
}

void UploadQueue::addFolder(const QString& folderPath, const QList<UploadFileItem>& files) {
    if (m_folderMap.contains(folderPath)) return;

    QTreeWidgetItem* folderItem = new QTreeWidgetItem(m_tree);
    folderItem->setText(0, QFileInfo(folderPath).fileName());
    folderItem->setText(1, QString("%1 个文件").arg(files.size()));
    folderItem->setText(2, "");
    folderItem->setCheckState(0, Qt::Checked);
    folderItem->setExpanded(true);

    UploadFolderItem* folderData = new UploadFolderItem;
    folderData->folderPath = folderPath;
    folderData->treeItem = folderItem;
    m_folderMap[folderPath] = folderData;

    for (const UploadFileItem& file : files) {
        QTreeWidgetItem* fileItem = new QTreeWidgetItem(folderItem);
        QString fname = QFileInfo(file.localPath).fileName();
        fileItem->setText(0, fname);
        fileItem->setText(1, formatSize(file.fileSize));
        fileItem->setText(2, "⚪ 待上传");
        fileItem->setForeground(2, QColor("#94A3B8"));
        fileItem->setCheckState(0, Qt::Checked);
        fileItem->setData(0, Qt::UserRole, file.recordId);

        UploadFileItem* fileData = new UploadFileItem(file);
        fileData->treeItem = fileItem;
        m_fileMap[file.recordId] = fileData;
        folderData->files.append(fileData);
    }
}

void UploadQueue::addFile(int recordId, const QString& drive,
                          const QString& /*usbPath*/, const QString& localPath, qint64 fileSize) {
    if (m_fileMap.contains(recordId)) return;

    QString folderPath = QFileInfo(localPath).absolutePath();
    QTreeWidgetItem* folderItem = nullptr;
    UploadFolderItem* folderData = nullptr;

    if (m_folderMap.contains(folderPath)) {
        folderData = m_folderMap[folderPath];
        folderItem = folderData->treeItem;
    } else {
        folderItem = new QTreeWidgetItem(m_tree);
        folderItem->setText(0, QFileInfo(folderPath).fileName());
        folderItem->setCheckState(0, Qt::Checked);
        folderItem->setExpanded(true);

        folderData = new UploadFolderItem;
        folderData->folderPath = folderPath;
        folderData->treeItem = folderItem;
        m_folderMap[folderPath] = folderData;
    }

    QTreeWidgetItem* fileItem = new QTreeWidgetItem(folderItem);
    QString fname = QFileInfo(localPath).fileName();
    fileItem->setText(0, fname);
    fileItem->setText(1, formatSize(fileSize));
    fileItem->setText(2, "⚪ 待上传");
    fileItem->setForeground(2, QColor("#94A3B8"));
    fileItem->setCheckState(0, Qt::Checked);
    fileItem->setData(0, Qt::UserRole, recordId);

    UploadFileItem* fileData = new UploadFileItem;
    fileData->recordId = recordId;
    fileData->localPath = localPath;
    fileData->fileSize = fileSize;
    fileData->status = "pending";
    fileData->treeItem = fileItem;
    m_fileMap[recordId] = fileData;
    folderData->files.append(fileData);
}

void UploadQueue::updateFileStatus(int recordId, const QString& status, qint64 uploadedBytes) {
    UploadFileItem* item = m_fileMap.value(recordId, nullptr);
    if (!item) return;

    QString fp = QFileInfo(item->localPath).absolutePath();
    UploadFolderItem* folder = m_folderMap.value(fp, nullptr);

    // If done/deleted/error, remove immediately from queue
    if (status == "uploaded" || status == "deleted" || status == "error") {
        // Remove from folder's file list
        if (folder) {
            folder->files.removeAll(item);
        }
        // Remove from map and delete
        m_fileMap.remove(recordId);
        delete item->treeItem;
        delete item;

        // If folder is empty, remove it too
        if (folder && folder->files.isEmpty()) {
            delete folder->treeItem;
            delete folder;
            m_folderMap.remove(fp);
        }
        return;
    }

    // Otherwise update the status display
    item->status = status;
    QTreeWidgetItem* treeItem = item->treeItem;

    if (status == "uploading") {
        if (uploadedBytes >= 0) {
            double pct = item->fileSize > 0 ? (uploadedBytes * 100.0 / item->fileSize) : 0;
            treeItem->setText(2, QString("🟡 上传中 %1%").arg(int(pct)));
        } else {
            treeItem->setText(2, "🟡 上传中");
        }
        treeItem->setForeground(2, QColor("#F59E0B"));
    } else if (status == "pending") {
        treeItem->setText(2, "⚪ 待上传");
        treeItem->setForeground(2, QColor("#94A3B8"));
    }
}

void UploadQueue::clearCompleted() {
    QList<int> toRemove;
    for (auto it = m_fileMap.begin(); it != m_fileMap.end(); ++it) {
        if (it.value()->status == "uploaded" || it.value()->status == "deleted" || it.value()->status == "error") {
            toRemove.append(it.key());
        }
    }
    for (int id : toRemove) {
        UploadFileItem* item = m_fileMap.take(id);
        if (item) {
            // Remove from folder's file list to avoid dangling pointer
            for (auto fit = m_folderMap.begin(); fit != m_folderMap.end(); ++fit) {
                fit.value()->files.removeAll(item);
            }
            delete item->treeItem;
            delete item;
        }
    }
    QStringList emptyFolders;
    for (auto it = m_folderMap.begin(); it != m_folderMap.end(); ++it) {
        if (it.value()->files.isEmpty()) {
            delete it.value()->treeItem;
            delete it.value();
            emptyFolders.append(it.key());
        }
    }
    for (const QString& fp : emptyFolders) {
        m_folderMap.remove(fp);
    }
}

void UploadQueue::clearAll() {
    m_tree->clear();
    qDeleteAll(m_fileMap);
    qDeleteAll(m_folderMap);
    m_fileMap.clear();
    m_folderMap.clear();
}

int UploadQueue::folderCount() const {
    return m_folderMap.size();
}

int UploadQueue::pendingCount() const {
    int count = 0;
    for (UploadFileItem* item : m_fileMap) {
        if (item->status == "pending" || item->status == "uploading") {
            count++;
        }
    }
    return count;
}
