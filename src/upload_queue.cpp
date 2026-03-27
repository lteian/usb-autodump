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

    m_tree = new QTreeWidget();
    m_tree->setHeaderLabels({"名称", "大小", "状态"});
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tree->setAlternatingRowColors(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setStyleSheet(R"(
        QTreeWidget {
            background: #2d2d2d;
            color: #d4d4d4;
            border: 1px solid #3c3c3c;
            border-radius: 4px;
            font-size: 12px;
        }
        QTreeWidget::item {
            padding: 2px 4px;
        }
        QTreeWidget::item:selected {
            background: #3c3c3c;
        }
        QHeaderView::section {
            background: #252525;
            color: #d4d4d4;
            border: 1px solid #3c3c3c;
            padding: 4px;
        }
    )");
    vl->addWidget(m_tree);
}

QString formatSize(qint64 b) {
    if (b >= 1024*1024*1024) return QString::number(b/1024.0/1024/1024,'f',1)+"GB";
    if (b >= 1024*1024) return QString::number(b/1024.0/1024,'f',1)+"MB";
    if (b >= 1024) return QString::number(b/1024.0,'f',1)+"KB";
    return QString::number(b)+"B";
}

void UploadQueue::addFolder(const QString& folderPath, const QList<UploadFileItem>& files) {
    if (m_folderMap.contains(folderPath)) return;

    // Create folder item
    QTreeWidgetItem* folderItem = new QTreeWidgetItem(m_tree);
    folderItem->setText(0, "📁 " + QFileInfo(folderPath).fileName());
    folderItem->setText(1, QString("%1 个文件").arg(files.size()));
    folderItem->setText(2, "");
    folderItem->setCheckState(0, Qt::Checked);
    folderItem->setExpanded(true);

    UploadFolderItem* folderData = new UploadFolderItem;
    folderData->folderPath = folderPath;
    folderData->treeItem = folderItem;
    m_folderMap[folderPath] = folderData;

    // Create file items
    for (const UploadFileItem& file : files) {
        QTreeWidgetItem* fileItem = new QTreeWidgetItem(folderItem);
        QString fname = QFileInfo(file.localPath).fileName();
        fileItem->setText(0, "  " + fname);
        fileItem->setText(1, formatSize(file.fileSize));
        fileItem->setText(2, "○待上传");
        fileItem->setForeground(2, QColor("#9e9e9e"));
        fileItem->setCheckState(0, Qt::Checked);
        fileItem->setData(0, Qt::UserRole, file.recordId);

        UploadFileItem* fileData = new UploadFileItem(file);
        fileData->treeItem = fileItem;
        m_fileMap[file.recordId] = fileData;
        folderData->files.append(fileData);
    }
}

void UploadQueue::updateFileStatus(int recordId, const QString& status, qint64 uploadedBytes) {
    UploadFileItem* item = m_fileMap.value(recordId, nullptr);
    if (!item) return;

    item->status = status;
    QTreeWidgetItem* treeItem = item->treeItem;

    if (status == "uploading") {
        if (uploadedBytes >= 0) {
            double pct = item->fileSize > 0 ? (uploadedBytes * 100.0 / item->fileSize) : 0;
            treeItem->setText(2, QString("→ 上传中 %1%").arg(int(pct)));
        } else {
            treeItem->setText(2, "→ 上传中");
        }
        treeItem->setForeground(2, QColor("#2196F3"));
    } else if (status == "uploaded") {
        treeItem->setText(2, "✓已上传");
        treeItem->setForeground(2, QColor("#4CAF50"));
        treeItem->setCheckState(0, Qt::Unchecked);
    } else if (status == "deleted") {
        treeItem->setText(2, "✓已删除");
        treeItem->setForeground(2, QColor("#4CAF50"));
        treeItem->setCheckState(0, Qt::Unchecked);
    } else if (status == "error") {
        treeItem->setText(2, "✗错误");
        treeItem->setForeground(2, QColor("#F44336"));
    } else if (status == "pending") {
        treeItem->setText(2, "○待上传");
        treeItem->setForeground(2, QColor("#9e9e9e"));
    }

    // Check if folder is complete
    QString folderPath = item->localPath;
    // Extract folder path
    QString fp = QFileInfo(item->localPath).absolutePath();
    UploadFolderItem* folder = m_folderMap.value(fp, nullptr);
    if (folder) {
        bool allDone = true;
        for (UploadFileItem* f : folder->files) {
            if (f->status != "uploaded" && f->status != "deleted") {
                allDone = false;
                break;
            }
        }
        if (allDone && !folder->completed) {
            folder->completed = true;
            folder->treeItem->setText(0, "📁 " + QFileInfo(folder->folderPath).fileName() + " ✓");
            folder->treeItem->setExpanded(false);
        }
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
