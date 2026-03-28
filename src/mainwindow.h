#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <QMap>
#include <QLabel>
#include <QTimer>
#include <QList>
#include <QJsonObject>

class USBCard;
class USBMonitor;
class LogPanel;
class UploadQueue;
class DumpProcess;
class FTPProcess;
class SettingsDialog;
class USBDevice;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onDeviceInserted(const USBDevice& dev);
    void onDeviceRemoved(const QString& drive);
    void onDumpScanStarted(const QString& drive);
    void onDumpScanDone(const QString& drive, int totalFiles, qint64 totalSize);
    void onDumpCopyProgress(const QString& drive, const QString& file,
                           int fileIndex, int fileTotal,
                           double fileProgress, double speedMBps, int etaSeconds);
    void onDumpCopyFileDone(const QString& drive, const QString& file,
                           const QString& localPath, const QString& relPath,
                           qint64 fileSize,
                           int fileIndex, int fileTotal);
    void onDumpCopyAllDone(const QString& drive);
    void onDumpError(const QString& drive, const QString& msg);
    void onDumpFinished(const QString& drive);

    void onFTPUploadProgress(int recordId, qint64 uploadedBytes, qint64 totalBytes);
    void onFTPUploadDone(int recordId);
    void onFTPFileDeleted(int recordId, const QString& localPath);
    void onFTPUploadError(int recordId, const QString& msg);
    void onFTPConnectedChanged(bool connected);
    void onFTPLog(const QString& msg);

    void onFormatClicked(const QString& drive);
    void onEjectClicked(const QString& drive);
    void onCancelDumpClicked(const QString& drive);
    void onSettingsClicked();
    void onScanLocalDirectory();
    void resumePendingUploads();
    void onAbout();
    void updateStatusBar();
    void updateQueueCount();

private:
    void allocateCard(const QString& drive, const USBDevice& dev);
    void releaseCard(const QString& drive);
    void startDumpProcess(const QString& drive);
    void updateDumpStatusLabel();
    QString formatSize(qint64 b) const;

    QList<USBCard*> m_cards;
    QMap<QString, USBCard*> m_driveToCard;

    USBMonitor* m_usbMonitor = nullptr;
    QMap<QString, DumpProcess*> m_dumpProcesses; // drive -> DumpProcess
    FTPProcess* m_ftpProcess = nullptr;

    LogPanel* m_logPanel = nullptr;
    UploadQueue* m_uploadQueue = nullptr;
    QLabel* m_ftpStatusLabel = nullptr;
    QLabel* m_dumpStatusLabel = nullptr;
    QLabel* m_queueCountLabel = nullptr;

    bool m_ftpConnected = false;
    int m_activeDumpCount = 0;
};

#endif // MAINWINDOW_H
