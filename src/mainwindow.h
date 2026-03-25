#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>
#include <QLabel>
#include <QTableWidget>
#include <QTimer>
#include <QList>

class USBCard;
class USBMonitor;
class CopyEngine;
class FTPUploader;
class LogPanel;
class USBDevice;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onDeviceInserted(const USBDevice& dev);
    void onDeviceRemoved(const QString& drive);
    void onCopyProgress(const QString& drive, int done, int total, const CopyTask& task);
    void onCopyDone(const QString& drive, const QList<CopyTask>& tasks);
    void onUploadDone(const UploadTask& task);
    void onFormatClicked(const QString& drive);
    void onEjectClicked(const QString& drive);
    void onCancelClicked(const QString& drive);
    void onSettingsClicked();
    void updateStatusBar();
    void refreshPendingList();

private:
    void allocateCard(const QString& drive, const USBDevice& dev);
    void releaseCard(const QString& drive);

    QList<USBCard*> m_cards;
    QMap<QString, USBCard*> m_driveToCard;

    USBMonitor* m_usbMonitor = nullptr;
    CopyEngine* m_copyEngine = nullptr;
    FTPUploader* m_ftpUploader = nullptr;
    LogPanel* m_logPanel = nullptr;
    QLabel* m_ftpStatusLabel = nullptr;
    QTableWidget* m_pendingList = nullptr;
};

#endif // MAINWINDOW_H
