#ifndef UPLOAD_QUEUE_H
#define UPLOAD_QUEUE_H

#include <QWidget>
#include <QTreeWidget>
#include <QString>
#include <QList>

struct UploadFileItem {
    int recordId = 0;
    QString localPath;
    QString remotePath;
    qint64 fileSize = 0;
    QString status; // pending/uploading/uploaded/deleted/error
    QTreeWidgetItem* treeItem = nullptr;
};

struct UploadFolderItem {
    QString folderPath;
    QTreeWidgetItem* treeItem = nullptr;
    QList<UploadFileItem*> files;
    bool completed = false;
};

class UploadQueue : public QWidget {
    Q_OBJECT
public:
    explicit UploadQueue(QWidget* parent = nullptr);

    // Add a folder with its files
    void addFolder(const QString& folderPath, const QList<UploadFileItem>& files);

    // Update file status
    void updateFileStatus(int recordId, const QString& status, qint64 uploadedBytes = -1);

    // Clear all items
    void clearAll();

    // Get folder count
    int folderCount() const;
    int pendingCount() const;

signals:
    void itemCheckChanged(int recordId, bool checked);

private:
    QTreeWidget* m_tree = nullptr;
    QMap<int, UploadFileItem*> m_fileMap; // recordId -> item
    QMap<QString, UploadFolderItem*> m_folderMap; // folderPath -> item
};

#endif // UPLOAD_QUEUE_H
