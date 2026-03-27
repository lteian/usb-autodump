#include "mainwindow.h"
#include "usb_card.h"
#include "log_panel.h"
#include "settings_dialog.h"
#include "password_dialog.h"
#include "usb_monitor.h"
#include "dump_process.h"
#include "ftp_process.h"
#include "disk_tool.h"
#include "file_record.h"
#include "upload_queue.h"
#include "config.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QStatusBar>
#include <QScrollArea>
#include <QDebug>
#include <QMessageBox>
#include <QGroupBox>
#include <QFileInfo>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("🚀 U盘自动转储工具");
    setMinimumSize(1000, 700);
    setStyleSheet("QMainWindow { background: #1e1e1e; } QLabel { color: #e0e0e0; }");

    // Central widget
    QWidget* central = new QWidget();
    setCentralWidget(central);
    QVBoxLayout* vl = new QVBoxLayout(central);
    vl->setSpacing(4);

    // ── Header ────────────────────────────────────────────
    QFrame* header = new QFrame();
    header->setStyleSheet("background: #252525; border-bottom: 1px solid #3c3c3c; padding: 8px;");
    QHBoxLayout* hl = new QHBoxLayout(header);

    QLabel* title = new QLabel("🚀 U盘自动转储工具");
    title->setStyleSheet("font-size: 16px; font-weight: bold; color: #ffffff;");
    hl->addWidget(title);
    hl->addStretch();

    m_dumpStatusLabel = new QLabel("转储: 0个进行中");
    m_dumpStatusLabel->setStyleSheet("color: #9e9e9e; font-size: 12px; padding: 0 8px;");
    hl->addWidget(m_dumpStatusLabel);

    m_ftpStatusLabel = new QLabel("FTP: ○未连接");
    m_ftpStatusLabel->setStyleSheet("color: #9e9e9e; font-size: 12px; padding: 0 8px;");
    hl->addWidget(m_ftpStatusLabel);

    QPushButton* settingsBtn = new QPushButton("⚙ 设置");
    settingsBtn->setStyleSheet("background: #424242; color: white; border: none; border-radius: 4px; padding: 6px 12px;");
    connect(settingsBtn, &QPushButton::clicked, this, &MainWindow::onSettingsClicked);
    hl->addWidget(settingsBtn);
    vl->addWidget(header);

    // ── USB Cards Area ────────────────────────────────────
    QScrollArea* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("background: transparent;");

    QWidget* cardsWidget = new QWidget();
    QGridLayout* cardsLayout = new QGridLayout(cardsWidget);
    cardsLayout->setSpacing(12);

    m_cards.reserve(8);
    for (int i = 0; i < 8; ++i) {
        USBCard* card = new USBCard();
        m_cards.append(card);
        cardsLayout->addWidget(card, i / 4, i % 4);
        connect(card, &USBCard::formatClicked, this, &MainWindow::onFormatClicked);
        connect(card, &USBCard::ejectClicked, this, &MainWindow::onEjectClicked);
        connect(card, &USBCard::cancelDumpClicked, this, &MainWindow::onCancelDumpClicked);
    }
    scroll->setWidget(cardsWidget);
    vl->addWidget(scroll, 1);

    // ── Upload Queue ──────────────────────────────────────
    QGroupBox* queueGrp = new QGroupBox("📁 待上传文件夹");
    queueGrp->setStyleSheet(R"(
        QGroupBox { color: #e0e0e0; border: 1px solid #4a4a4a; border-radius: 4px; margin-top: 4px; }
        QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }
    )");
    QVBoxLayout* queueVl = new QVBoxLayout(queueGrp);
    m_uploadQueue = new UploadQueue();
    queueVl->addWidget(m_uploadQueue);
    vl->addWidget(queueGrp);

    // ── Log Panel ─────────────────────────────────────────
    m_logPanel = new LogPanel();
    vl->addWidget(m_logPanel);

    // ── Status Bar ────────────────────────────────────────
    QStatusBar* sb = new QStatusBar();
    setStatusBar(sb);

    // ── USB Monitor ───────────────────────────────────────
    m_usbMonitor = new USBMonitor(this);
    connect(m_usbMonitor, &USBMonitor::deviceInserted, this, &MainWindow::onDeviceInserted);
    connect(m_usbMonitor, &USBMonitor::deviceRemoved, this, &MainWindow::onDeviceRemoved);

    // ── FTP Process ───────────────────────────────────────
    m_ftpProcess = new FTPProcess(this);
    connect(m_ftpProcess, &FTPProcess::uploadProgress, this, &MainWindow::onFTPUploadProgress);
    connect(m_ftpProcess, &FTPProcess::uploadDone, this, &MainWindow::onFTPUploadDone);
    connect(m_ftpProcess, &FTPProcess::fileDeleted, this, &MainWindow::onFTPFileDeleted);
    connect(m_ftpProcess, &FTPProcess::uploadError, this, &MainWindow::onFTPUploadError);
    connect(m_ftpProcess, &FTPProcess::connectedChanged, this, &MainWindow::onFTPConnectedChanged);
    connect(m_ftpProcess, &FTPProcess::logMessage, this, &MainWindow::onFTPLog);

    // Initial scan
    for (const USBDevice& dev : m_usbMonitor->currentDevices()) {
        QTimer::singleShot(500, this, [this, dev]() {
            onDeviceInserted(dev);
        });
    }

    // Status bar timer
    QTimer* statusTimer = new QTimer(this);
    connect(statusTimer, &QTimer::timeout, this, &MainWindow::updateStatusBar);
    statusTimer->start(3000);

    m_logPanel->appendInfo("服务已启动，等待 USB 设备...");
}

MainWindow::~MainWindow() {
    // Kill all dump processes
    for (DumpProcess* proc : m_dumpProcesses.values()) {
        proc->sendCancel();
    }
    if (m_ftpProcess) {
        m_ftpProcess->sendCancelAll();
    }
}

void MainWindow::onDeviceInserted(const USBDevice& dev) {
    QString drive = dev.driveLetter;
    double usedPct = dev.totalSize > 0 ? (dev.usedSize() * 100.0 / dev.totalSize) : 0;
    m_logPanel->appendInfo(QString("%1 插入 | 容量 %2GB | 已用 %3% | 剩余 %4")
        .arg(drive)
        .arg(dev.totalSize / 1024.0 / 1024 / 1024, 0, 'f', 1)
        .arg(int(usedPct))
        .arg(formatSize(dev.freeSpace)));
    allocateCard(drive, dev);
}

void MainWindow::onDeviceRemoved(const QString& drive) {
    m_logPanel->appendInfo(drive + " 已移除");
    releaseCard(drive);
}

void MainWindow::allocateCard(const QString& drive, const USBDevice& dev) {
    for (USBCard* card : m_cards) {
        if (card->property("drive").toString().isEmpty()) {
            m_driveToCard[drive] = card;
            card->setProperty("drive", drive);
            card->setDrive(drive, dev.label, dev.totalSize, dev.usedSize(), dev.freeSpace);
            card->setStatus("copying");
            startDumpProcess(drive);
            return;
        }
    }
}

void MainWindow::startDumpProcess(const QString& drive) {
    if (m_dumpProcesses.contains(drive)) return;

    DumpProcess* proc = new DumpProcess(drive, this);
    m_dumpProcesses[drive] = proc;
    m_activeDumpCount++;
    updateDumpStatusLabel();

    connect(proc, &DumpProcess::scanStarted, this, &MainWindow::onDumpScanStarted);
    connect(proc, &DumpProcess::scanDone, this, &MainWindow::onDumpScanDone);
    connect(proc, &DumpProcess::copyProgress, this, &MainWindow::onDumpCopyProgress);
    connect(proc, &DumpProcess::copyFileDone, this, &MainWindow::onDumpCopyFileDone);
    connect(proc, &DumpProcess::copyAllDone, this, &MainWindow::onDumpCopyAllDone);
    connect(proc, &DumpProcess::error, this, &MainWindow::onDumpError);
    connect(proc, &DumpProcess::finished, this, [this, drive]() {
        onDumpFinished(drive);
    });
}

void MainWindow::releaseCard(const QString& drive) {
    // Cancel dump process if running
    DumpProcess* proc = m_dumpProcesses.take(drive);
    if (proc) {
        proc->sendCancel();
        proc->deleteLater();
        m_activeDumpCount--;
        updateDumpStatusLabel();
    }

    USBCard* card = m_driveToCard.take(drive);
    if (card) {
        card->clear();
        card->setProperty("drive", QString());
    }
}

void MainWindow::onDumpScanStarted(const QString& drive) {
    USBCard* card = m_driveToCard.value(drive, nullptr);
    if (card) {
        m_logPanel->appendInfo(drive + " 正在扫描视频文件...");
    }
}

void MainWindow::onDumpScanDone(const QString& drive, int totalFiles, qint64 totalSize) {
    USBCard* card = m_driveToCard.value(drive, nullptr);
    if (card) {
        m_logPanel->appendInfo(QString("%1 扫描完成: %2 个文件 (%3)")
            .arg(drive).arg(totalFiles).arg(formatSize(totalSize)));
    }
}

void MainWindow::onDumpCopyProgress(const QString& drive, const QString& file,
                                    int fileIndex, int fileTotal,
                                    double fileProgress, double speedMBps, int etaSeconds) {
    USBCard* card = m_driveToCard.value(drive, nullptr);
    if (card) {
        // Extract filename
        QString fname = file;
        int p = fname.lastIndexOf('/');
        if (p < 0) p = fname.lastIndexOf('\\');
        if (p >= 0) fname = fname.mid(p + 1);

        card->updateProgress(fileIndex, fileTotal, fileProgress, speedMBps, etaSeconds, fname);
    }
}

void MainWindow::onDumpCopyFileDone(const QString& drive, const QString& file,
                                    int fileIndex, int fileTotal) {
    USBCard* card = m_driveToCard.value(drive, nullptr);
    if (!card) return;

    // Add to upload queue
    QString fname = file;
    int p = fname.lastIndexOf('/');
    if (p < 0) p = fname.lastIndexOf('\\');
    if (p >= 0) fname = fname.mid(p + 1);

    Config& cfg = Config::instance();
    cfg.load();
    QString localPath = cfg.localPath(drive);
    QString subPath = cfg.ftpConfig().value("sub_path").toString("/");

    // Find the record for this file
    QList<FileRecord> pending = FileRecordDB::instance().pendingRecords();
    for (const FileRecord& rec : pending) {
        if (rec.localPath.contains(fname) || rec.filePath == file) {
            // Send to FTP process
            FTPUploadTask task;
            task.recordId = rec.id;
            task.localPath = rec.localPath;
            task.remotePath = subPath + "/" + QFileInfo(rec.localPath).fileName();
            task.fileSize = rec.fileSize;
            task.status = "pending";
            m_ftpProcess->sendUpload(task);
            m_uploadQueue->updateFileStatus(rec.id, "uploading");
            break;
        }
    }

    m_logPanel->appendDebug(QString("%1 [%2/%3] 复制完成: %4")
        .arg(drive).arg(fileIndex).arg(fileTotal).arg(fname));
}

void MainWindow::onDumpCopyAllDone(const QString& drive) {
    USBCard* card = m_driveToCard.value(drive, nullptr);
    if (card) {
        card->setStatus("done");
        m_logPanel->appendInfo(drive + " 全部复制完成");
    }
    updateStatusBar();
}

void MainWindow::onDumpError(const QString& drive, const QString& msg) {
    USBCard* card = m_driveToCard.value(drive, nullptr);
    if (card) {
        card->setStatus("done");
    }
    m_logPanel->appendError(drive + " 错误: " + msg);
}

void MainWindow::onDumpFinished(const QString& drive) {
    DumpProcess* proc = m_dumpProcesses.take(drive);
    if (proc) {
        proc->deleteLater();
        m_activeDumpCount--;
        updateDumpStatusLabel();
    }
}

void MainWindow::updateDumpStatusLabel() {
    if (m_dumpStatusLabel) {
        m_dumpStatusLabel->setText(QString("转储: %1个进行中").arg(m_activeDumpCount));
    }
}

void MainWindow::onFTPUploadProgress(int recordId, qint64 uploadedBytes, qint64 totalBytes) {
    m_uploadQueue->updateFileStatus(recordId, "uploading", uploadedBytes);
}

void MainWindow::onFTPUploadDone(int recordId) {
    m_uploadQueue->updateFileStatus(recordId, "uploaded");
    FileRecordDB::instance().updateStatus(recordId, "uploaded");
    m_logPanel->appendInfo("上传成功 [record=" + QString::number(recordId) + "]");
    updateStatusBar();
}

void MainWindow::onFTPFileDeleted(int recordId) {
    m_uploadQueue->updateFileStatus(recordId, "deleted");
    FileRecordDB::instance().updateStatus(recordId, "deleted");
    m_logPanel->appendInfo("本地文件已删除 [record=" + QString::number(recordId) + "]");
}

void MainWindow::onFTPUploadError(int recordId, const QString& msg) {
    m_uploadQueue->updateFileStatus(recordId, "error");
    FileRecordDB::instance().updateStatus(recordId, "error", msg);
    m_logPanel->appendError("上传失败 [record=" + QString::number(recordId) + "]: " + msg);
    updateStatusBar();
}

void MainWindow::onFTPConnectedChanged(bool connected) {
    m_ftpConnected = connected;
    if (connected) {
        m_ftpStatusLabel->setText("FTP: ●已连接");
        m_ftpStatusLabel->setStyleSheet("color: #4CAF50; font-size: 12px; padding: 0 8px;");
    } else {
        m_ftpStatusLabel->setText("FTP: ○未连接");
        m_ftpStatusLabel->setStyleSheet("color: #9e9e9e; font-size: 12px; padding: 0 8px;");
    }
}

void MainWindow::onFTPLog(const QString& msg) {
    m_logPanel->appendDebug(msg);
}

void MainWindow::onFormatClicked(const QString& drive) {
    USBCard* card = m_driveToCard.value(drive, nullptr);
    if (!card) return;

    int r = QMessageBox::question(this, "确认格式化",
        QString("确定要格式化 %1 吗？所有数据将被清除！").arg(drive),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (r != QMessageBox::Yes) return;

    card->setStatus("formatting");
    m_logPanel->appendWarning("开始格式化 " + drive + " ...");

    bool ok = DiskTool::formatDrive(drive);
    if (ok) {
        m_logPanel->appendInfo("格式化成功: " + drive);
    } else {
        m_logPanel->appendError("格式化失败: " + drive);
    }
    if (card) card->setStatus("done");
}

void MainWindow::onEjectClicked(const QString& drive) {
    m_logPanel->appendInfo("弹出 " + drive + " ...");
    bool ok = DiskTool::ejectDrive(drive);
    m_logPanel->appendInfo(ok ? ("已弹出: " + drive) : ("弹出失败: " + drive));
}

void MainWindow::onCancelDumpClicked(const QString& drive) {
    DumpProcess* proc = m_dumpProcesses.value(drive, nullptr);
    if (proc) {
        proc->sendCancel();
        m_logPanel->appendWarning("已取消复制: " + drive);
    }
    USBCard* card = m_driveToCard.value(drive, nullptr);
    if (card) card->setStatus("idle");
}

void MainWindow::onSettingsClicked() {
    Config& cfg = Config::instance();
    cfg.load();

    // First check if password is set
    if (!cfg.isPasswordSet()) {
        // No password set - ask to set one
        PasswordDialog dlg(PasswordDialog::SetNew, this);
        if (dlg.exec() == QDialog::Accepted) {
            // Password set, now open settings
            SettingsDialog settingsDlg(this);
            settingsDlg.exec();
        }
        return;
    }

    // Password is set - ask for it
    PasswordDialog dlg(PasswordDialog::Verify, this);
    int result = dlg.exec();

    if (dlg.wasReset()) {
        // User reset password - show main window again
        QMessageBox::information(this, "已重置", "配置已清空，请重启程序。");
        QApplication::quit();
        return;
    }

    if (result == QDialog::Accepted) {
        SettingsDialog settingsDlg(this);
        settingsDlg.exec();
    }
}

void MainWindow::updateStatusBar() {
    auto pending = FileRecordDB::instance().pendingCountAndSize();
    auto uploaded = FileRecordDB::instance().uploadedCountAndSize();

    statusBar()->showMessage(
        QString("  待上传: %1个 (%2)  |  已上传: %3个 (%4)  |  FTP: %5")
            .arg(pending.first).arg(formatSize(pending.second))
            .arg(uploaded.first).arg(formatSize(uploaded.second))
            .arg(m_ftpConnected ? "●已连接" : "○未连接")
    );
}

QString MainWindow::formatSize(qint64 b) const {
    if (b >= 1024*1024*1024) return QString::number(b/1024.0/1024/1024,'f',1) + "GB";
    if (b >= 1024*1024) return QString::number(b/1024.0/1024,'f',1) + "MB";
    if (b >= 1024) return QString::number(b/1024.0,'f',1) + "KB";
    return QString::number(b) + "B";
}
