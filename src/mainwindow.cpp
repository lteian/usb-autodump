#include "mainwindow.h"
#include "disk_space_widget.h"
#include "usb_card.h"
#include "log_panel.h"
#include "settings_dialog.h"
#include "password_dialog.h"
#include "usb_monitor.h"
#include "dump_process.h"
#include "ftp_pool.h"
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
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QQueue>
#include <QApplication>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QTimer>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("U盘自动转储工具");
    setMinimumSize(960, 680);
    setStyleSheet("QMainWindow { background: #F1F5F9; }");

    QWidget* central = new QWidget();
    setCentralWidget(central);

    QMenuBar* menuBar = new QMenuBar(this);
    setMenuBar(menuBar);
    QMenu* helpMenu = menuBar->addMenu("帮助");
    QAction* aboutAction = helpMenu->addAction("关于...");
    connect(aboutAction, &QAction::triggered, this, &MainWindow::onAbout);

    QVBoxLayout* vl = new QVBoxLayout(central);
    vl->setSpacing(10);
    vl->setContentsMargins(20, 20, 20, 10);

    // ── Header (Compact single row) ──────────────────────
    QFrame* headerCard = new QFrame();
    headerCard->setObjectName("headerCard");
    headerCard->setStyleSheet(R"(
        QFrame#headerCard {
            background: #FFFFFF;
            border-radius: 10px;
            padding: 10px 16px;
        }
    )");
    QHBoxLayout* headerHl = new QHBoxLayout(headerCard);
    headerHl->setContentsMargins(0, 0, 0, 0);
    headerHl->setSpacing(16);

    QLabel* titleLabel = new QLabel("U盘自动转储工具");
    titleLabel->setStyleSheet("color: #1F2329; font-size: 15px; font-weight: 700; font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif;");
    headerHl->addWidget(titleLabel);

    m_dumpStatusLabel = new QLabel("转储: 0个进行中");
    m_dumpStatusLabel->setStyleSheet("color: #86909C; font-size: 12px; font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif;");
    headerHl->addWidget(m_dumpStatusLabel);

    m_ftpStatusLabel = new QLabel("FTP: 未连接");
    m_ftpStatusLabel->setStyleSheet("color: #86909C; font-size: 12px; font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif;");
    headerHl->addWidget(m_ftpStatusLabel);
    headerHl->addStretch();

    QPushButton* settingsBtn = new QPushButton("设置");
    settingsBtn->setStyleSheet(R"(
        QPushButton {
            background: #FFFFFF;
            color: #475569;
            border: 1px solid #E2E8F0;
            border-radius: 8px;
            padding: 6px 14px;
            font-size: 12px;
            font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif;
        }
        QPushButton:hover { background: #F1F5F9; border-color: #3B82F6; }
    )");
    connect(settingsBtn, &QPushButton::clicked, this, &MainWindow::onSettingsClicked);
    headerHl->addWidget(settingsBtn);

    QPushButton* scanLocalBtn = new QPushButton("扫描");
    scanLocalBtn->setStyleSheet(R"(
        QPushButton {
            background: #3B82F6;
            color: white;
            border: none;
            border-radius: 8px;
            padding: 6px 14px;
            font-size: 12px;
            font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif;
            font-weight: 500;
        }
        QPushButton:hover { background: #2563EB; }
    )");
    connect(scanLocalBtn, &QPushButton::clicked, this, &MainWindow::onScanLocalDirectory);
    headerHl->addWidget(scanLocalBtn);
    vl->addWidget(headerCard);

    // ── Main Content: Two Columns ────────────────────────
    QHBoxLayout* mainHl = new QHBoxLayout();
    mainHl->setSpacing(10);
    mainHl->setStretchFactor(mainHl, 1);

    // Left Column: USB Cards
    QFrame* leftCard = new QFrame();
    leftCard->setObjectName("leftCard");
    leftCard->setStyleSheet(R"(
        QFrame#leftCard {
            background: #FFFFFF;
            border-radius: 12px;
            padding: 12px;
        }
    )");
    QVBoxLayout* leftVl = new QVBoxLayout(leftCard);
    leftVl->setSpacing(6);
    leftVl->setContentsMargins(0, 0, 0, 0);

    QLabel* leftTitle = new QLabel("USB 设备");
    leftTitle->setStyleSheet("color: #1F2329; font-size: 13px; font-weight: 600; font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif;");
    leftVl->addWidget(leftTitle);

    m_cards.reserve(4);
    for (int i = 0; i < 4; ++i) {
        USBCard* card = new USBCard();
        card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_cards.append(card);
        leftVl->addWidget(card, 1);  // stretch 1 = equal 25% each
        connect(card, &USBCard::formatClicked, this, &MainWindow::onFormatClicked);
        connect(card, &USBCard::ejectClicked, this, &MainWindow::onEjectClicked);
        connect(card, &USBCard::cancelDumpClicked, this, &MainWindow::onCancelDumpClicked);
    }
    leftVl->addStretch(3);  // add stretch at bottom
    mainHl->addWidget(leftCard, 3);  // 30%

    // Middle Column: Pie Chart (80%) + Log (20%)
    QFrame* middleCard = new QFrame();
    middleCard->setObjectName("middleCard");
    middleCard->setStyleSheet(R"(
        QFrame#middleCard {
            background: #FFFFFF;
            border-radius: 12px;
            padding: 12px;
        }
    )");
    QVBoxLayout* middleVl = new QVBoxLayout(middleCard);
    middleVl->setSpacing(8);
    middleVl->setContentsMargins(0, 0, 0, 0);

    // Top 80%: Disk Space Pie Chart
    QLabel* diskTitle = new QLabel("本地存储空间");
    diskTitle->setStyleSheet("color: #1F2329; font-size: 13px; font-weight: 600; font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif;");
    middleVl->addWidget(diskTitle);

    m_diskSpaceWidget = new DiskSpaceWidget();
    m_diskSpaceWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_diskSpaceWidget->setMinimumSize(100, 100);
    middleVl->addWidget(m_diskSpaceWidget, 4);  // stretch 4 = 80%

    m_diskSpaceLabel = new QLabel("磁盘: C:\\");
    m_diskSpaceLabel->setStyleSheet("color: #86909C; font-size: 11px; font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif;");
    middleVl->addWidget(m_diskSpaceLabel);

    m_diskSpaceDetailLabel = new QLabel("共 0GB | 可用 0GB");
    m_diskSpaceDetailLabel->setStyleSheet("color: #4E5969; font-size: 12px; font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif; background: transparent;");
    middleVl->addWidget(m_diskSpaceDetailLabel);

    // Bottom 20%: Log Panel
    QLabel* logTitle = new QLabel("运行日志");
    logTitle->setStyleSheet("color: #1F2329; font-size: 13px; font-weight: 600; font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif;");
    middleVl->addWidget(logTitle);

    m_logPanel = new LogPanel();
    m_logPanel->setMinimumHeight(60);
    middleVl->addWidget(m_logPanel, 1);  // stretch 1 = 20%
    mainHl->addWidget(middleCard, 4);  // 40%

    // Right Column: Upload Queue (30%)
    QFrame* rightCard = new QFrame();
    rightCard->setObjectName("rightCard");
    rightCard->setStyleSheet(R"(
        QFrame#rightCard {
            background: #FFFFFF;
            border-radius: 12px;
            padding: 12px;
        }
    )");
    QVBoxLayout* rightVl = new QVBoxLayout(rightCard);
    rightVl->setSpacing(8);
    rightVl->setContentsMargins(0, 0, 0, 0);

    QLabel* queueTitle = new QLabel("待上传队列");
    queueTitle->setStyleSheet("color: #1F2329; font-size: 13px; font-weight: 600; font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif;");
    rightVl->addWidget(queueTitle);

    QHBoxLayout* queueHeader = new QHBoxLayout();
    queueHeader->setContentsMargins(0, 0, 0, 0);
    m_queueCountLabel = new QLabel("0个文件夹 · 0个待上传");
    m_queueCountLabel->setStyleSheet("color: #86909C; font-size: 11px; font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif;");
    queueHeader->addWidget(m_queueCountLabel);
    queueHeader->addStretch();

    QPushButton* clearBtn = new QPushButton("清空已完成");
    clearBtn->setStyleSheet(R"(
        QPushButton {
            background: transparent;
            color: #64748B;
            border: 1px solid #E2E8F0;
            border-radius: 6px;
            padding: 3px 10px;
            font-size: 11px;
            font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif;
        }
        QPushButton:hover { color: #3B82F6; background: #F1F5F9; border-color: #3B82F6; }
    )");
    queueHeader->addWidget(clearBtn);
    rightVl->addLayout(queueHeader);

    m_uploadQueue = new UploadQueue();
    connect(clearBtn, &QPushButton::clicked, m_uploadQueue, &UploadQueue::clearCompleted);
    rightVl->addWidget(m_uploadQueue, 1);
    mainHl->addWidget(rightCard, 3);  // 30%

    vl->addLayout(mainHl, 1);

    // ── Status Bar ────────────────────────────────────────
    QStatusBar* sb = new QStatusBar();
    sb->setStyleSheet("color: #86909C; font-size: 12px; font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif; background: #FFFFFF; border-top: none;");
    setStatusBar(sb);

    // ── USB Monitor ───────────────────────────────────────
    m_usbMonitor = new USBMonitor(this);
    connect(m_usbMonitor, &USBMonitor::deviceInserted, this, &MainWindow::onDeviceInserted);
    connect(m_usbMonitor, &USBMonitor::deviceRemoved, this, &MainWindow::onDeviceRemoved);

    // ── FTP Process (5 parallel connections) ───────────────
    m_ftpProcess = new FTPConnectionPool(this, 5);
    connect(m_ftpProcess, &FTPConnectionPool::uploadProgress, this, &MainWindow::onFTPUploadProgress);
    connect(m_ftpProcess, &FTPConnectionPool::uploadDone, this, &MainWindow::onFTPUploadDone);
    connect(m_ftpProcess, &FTPConnectionPool::fileDeleted, this, &MainWindow::onFTPFileDeleted);
    connect(m_ftpProcess, &FTPConnectionPool::uploadError, this, &MainWindow::onFTPUploadError);
    connect(m_ftpProcess, &FTPConnectionPool::connectedChanged, this, &MainWindow::onFTPConnectedChanged);
    connect(m_ftpProcess, &FTPConnectionPool::logMessage, this, &MainWindow::onFTPLog);

    // ── Disk Tool (for async format) ─────────────────────────
    connect(diskToolInstance(), &DiskTool::formatFinished, this, &MainWindow::onFormatFinished);

    for (const USBDevice& dev : m_usbMonitor->currentDevices()) {
        QTimer::singleShot(500, this, [this, dev]() {
            onDeviceInserted(dev);
        });
    }

    QTimer* statusTimer = new QTimer(this);
    connect(statusTimer, &QTimer::timeout, this, &MainWindow::updateStatusBar);
    statusTimer->start(3000);

    QTimer* autoScanTimer = new QTimer(this);
    connect(autoScanTimer, &QTimer::timeout, this, &MainWindow::onScanLocalDirectory);
    autoScanTimer->start(30000);

    QTimer::singleShot(1000, this, &MainWindow::onScanLocalDirectory);

    resumePendingUploads();
    updateQueueCount();

    // Disk space update timer
    QTimer* diskTimer = new QTimer(this);
    connect(diskTimer, &QTimer::timeout, this, &MainWindow::updateDiskSpace);
    diskTimer->start(5000);
    updateDiskSpace();

    m_logPanel->appendInfo("服务已启动，等待 USB 设备...");
}

MainWindow::~MainWindow() {
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
    QString info = QString("%1 插入 | 容量 %2GB | 已用 %3% | 剩余 %4")
        .arg(drive)
        .arg(dev.totalSize / 1024.0 / 1024 / 1024, 0, 'f', 1)
        .arg(int(usedPct))
        .arg(formatSize(dev.freeSpace));
    m_logPanel->appendInfo(info);
    qInfo() << info;
    allocateCard(drive, dev);
}

void MainWindow::onDeviceRemoved(const QString& drive) {
    QString info = drive + " 已移除";
    m_logPanel->appendInfo(info);
    qInfo() << info;
    releaseCard(drive);
}

void MainWindow::allocateCard(const QString& drive, const USBDevice& dev) {
    if (drive.isEmpty()) return;
    if (m_driveToCard.contains(drive)) return;
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
    if (card) m_logPanel->appendInfo(drive + " 正在扫描视频文件...");
}

void MainWindow::onDumpScanDone(const QString& drive, int totalFiles, qint64 totalSize) {
    USBCard* card = m_driveToCard.value(drive, nullptr);
    m_driveHasFiles[drive] = (totalFiles > 0);
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
        QString fname = file;
        int p = fname.lastIndexOf('/');
        if (p < 0) p = fname.lastIndexOf('\\');
        if (p >= 0) fname = fname.mid(p + 1);
        card->updateProgress(fileIndex, fileTotal, fileProgress, speedMBps, etaSeconds, fname);
    }
}

void MainWindow::onDumpCopyFileDone(const QString& drive, const QString& file,
                                    const QString& localPath, const QString& relPath,
                                    qint64 fileSize,
                                    int fileIndex, int fileTotal) {
    USBCard* card = m_driveToCard.value(drive, nullptr);
    if (!card) {
        return;
    }
    m_driveHasFiles[drive] = true;
    QString fname = file;
    int p = fname.lastIndexOf('/');
    if (p < 0) p = fname.lastIndexOf('\\');
    if (p >= 0) fname = fname.mid(p + 1);

    FileRecord rec;
    rec.usbDrive = drive;
    rec.filePath = relPath;
    rec.localPath = localPath;
    rec.fileSize = fileSize;
    rec.status = "pending";
    int recordId = FileRecordDB::instance().add(rec);

    m_uploadQueue->addFile(recordId, drive, file, localPath, fileSize);
    updateQueueCount();

    Config& cfg = Config::instance();
    cfg.load();
    QString subPath = cfg.ftpConfig().value("sub_path").toString("/");
    QString remotePath = subPath + "/" + relPath;

    FTPUploadTask task;
    task.recordId = recordId;
    task.localPath = localPath;
    task.remotePath = remotePath;
    task.fileSize = fileSize;
    task.status = "pending";
    m_ftpProcess->sendUpload(task);
    m_uploadQueue->updateFileStatus(recordId, "uploading");
    m_logPanel->appendDebug(QString("%1 [%2/%3] 复制完成，等待上传: %4")
        .arg(drive).arg(fileIndex).arg(fileTotal).arg(fname));
}

void MainWindow::onDumpCopyAllDone(const QString& drive) {
    USBCard* card = m_driveToCard.value(drive, nullptr);
    if (card) {
        if (!m_driveHasFiles.value(drive, false)) {
            card->setStatus("no_files");
            m_logPanel->appendInfo(drive + " 无视频文件");
        } else {
            card->setStatus("done");
            m_logPanel->appendInfo(drive + " 转储完成");
        }
    }
    updateStatusBar();
}

void MainWindow::onDumpError(const QString& drive, const QString& msg) {
    USBCard* card = m_driveToCard.value(drive, nullptr);
    if (card) card->setStatus("done");
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
    if (m_dumpStatusLabel)
        m_dumpStatusLabel->setText(QString("转储: %1个进行中").arg(m_activeDumpCount));
}

void MainWindow::updateQueueCount() {
    if (!m_queueCountLabel) return;
    int folders = m_uploadQueue->folderCount();
    int pending = m_uploadQueue->pendingCount();
    m_queueCountLabel->setText(QString("待上传 (%1个文件夹 · %2个待上传)")
        .arg(folders).arg(pending));
}

void MainWindow::onFTPUploadProgress(int recordId, qint64 uploadedBytes, qint64) {
    m_uploadQueue->updateFileStatus(recordId, "uploading", uploadedBytes);
}

void MainWindow::onFTPUploadDone(int recordId) {
    m_uploadQueue->updateFileStatus(recordId, "uploaded");
    FileRecordDB::instance().updateStatus(recordId, "uploaded");
    updateStatusBar();
    updateQueueCount();
}

void MainWindow::onFTPFileDeleted(int recordId, const QString& localPath) {
    m_uploadQueue->updateFileStatus(recordId, "deleted");
    FileRecordDB::instance().updateStatus(recordId, "deleted");
    QString info = "上传成功：" + QFileInfo(localPath).fileName();
    m_logPanel->appendInfo(info);
    qInfo() << info;
    updateStatusBar();
    updateQueueCount();
}

void MainWindow::onFTPUploadError(int recordId, const QString& msg) {
    m_uploadQueue->updateFileStatus(recordId, "error");
    FileRecordDB::instance().updateStatus(recordId, "error", msg);
    QString err = "上传失败 [record=" + QString::number(recordId) + "]: " + msg;
    m_logPanel->appendError(err);
    qWarning() << err;
    updateStatusBar();
    updateQueueCount();
}

void MainWindow::onFTPConnectedChanged(bool connected) {
    m_ftpConnected = connected;
    if (connected) {
        m_ftpStatusLabel->setText("FTP: 已连接");
        m_ftpStatusLabel->setStyleSheet("color: #00B42A; font-size: 13px; font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif; font-weight: 500;");
    } else {
        m_ftpStatusLabel->setText("FTP: 未连接");
        m_ftpStatusLabel->setStyleSheet("color: #86909C; font-size: 13px; font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif;");
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
    DiskTool::asyncFormatDrive(drive);
}

void MainWindow::onFormatFinished(const QString& drive, bool ok, const QString& error) {
    Q_UNUSED(error);
    USBCard* card = m_driveToCard.value(drive, nullptr);
    if (ok) {
        // Copy license file to USB root
        QString appDir = QCoreApplication::applicationDirPath();
        QString licenseSrc = appDir + "/license.txt";
        QString licenseDst = drive + "/license.txt";
        if (QFile::exists(licenseSrc)) {
            if (QFile::copy(licenseSrc, licenseDst)) {
                m_logPanel->appendInfo("已复制授权文件到 " + drive);
            } else {
                m_logPanel->appendWarning("复制授权文件失败: " + licenseSrc);
            }
        } else {
            m_logPanel->appendWarning("未找到授权文件: " + licenseSrc);
        }
        if (card) card->setStatus("no_files");
    } else {
        if (card) card->setStatus("done");
    }
    m_logPanel->appendInfo(ok ? ("格式化成功: " + drive) : ("格式化失败: " + drive));
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
    if (!cfg.isPasswordSet()) {
        PasswordDialog dlg(PasswordDialog::SetNew, this);
        if (dlg.exec() == QDialog::Accepted) {
            SettingsDialog settingsDlg(this);
            settingsDlg.exec();
        }
        return;
    }
    PasswordDialog dlg(PasswordDialog::Verify, this);
    int result = dlg.exec();
    if (dlg.wasReset()) {
        QMessageBox::information(this, "已重置", "配置已清空，请重启程序。");
        QApplication::quit();
        return;
    }
    if (result == QDialog::Accepted) {
        SettingsDialog settingsDlg(this);
        settingsDlg.exec();
    }
}

void MainWindow::onScanLocalDirectory() {
    Config& cfg = Config::instance();
    cfg.load();
    QString localPath = cfg.localPath();
    QDir dir(localPath);
    if (!dir.exists()) {
        m_logPanel->appendWarning("待上传目录不存在: " + localPath);
        return;
    }
    QStringList videoExts = cfg.videoExtensions();
    QString subPath = cfg.ftpConfig().value("sub_path").toString("/");
    QQueue<QString> dirs;
    dirs.enqueue(localPath);
    int count = 0;

    while (!dirs.isEmpty()) {
        QString curDir = dirs.dequeue();
        QDir d(curDir);
        if (!d.exists()) continue;
        QFileInfoList entries = d.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
        for (const QFileInfo& fi : entries) {
            if (fi.isDir()) {
                dirs.enqueue(fi.absoluteFilePath());
            } else if (fi.isFile()) {
                QString ext = "." + fi.suffix().toLower();
                if (!videoExts.contains(ext)) continue;
                QString absPath = fi.absoluteFilePath();
                QString relPath = QDir(localPath).relativeFilePath(absPath);
                if (FileRecordDB::instance().hasPendingUpload(absPath)) continue;
                FileRecord rec;
                rec.usbDrive = "LOCAL";
                rec.filePath = relPath;
                rec.localPath = absPath;
                rec.fileSize = fi.size();
                rec.status = "pending";
                int recordId = FileRecordDB::instance().add(rec);
                m_uploadQueue->addFile(recordId, "LOCAL", relPath, absPath, fi.size());
                QString remotePath = subPath + "/" + relPath;
                FTPUploadTask task;
                task.recordId = recordId;
                task.localPath = absPath;
                task.remotePath = remotePath;
                task.fileSize = fi.size();
                task.status = "pending";
                m_ftpProcess->sendUpload(task);
                m_uploadQueue->updateFileStatus(recordId, "uploading");
                count++;
            }
        }
    }
    updateQueueCount();
    if (count > 0)
        m_logPanel->appendInfo(QString("已扫描到 %1 个待上传文件").arg(count));
    else
        m_logPanel->appendInfo("待上传目录为空: " + localPath);
}

void MainWindow::updateStatusBar() {
    auto pending = FileRecordDB::instance().pendingCountAndSize();
    auto uploaded = FileRecordDB::instance().uploadedCountAndSize();
    statusBar()->showMessage(
        QString("待上传: %1个 (%2)  |  已上传: %3个 (%4)  |  FTP: %5")
            .arg(pending.first).arg(formatSize(pending.second))
            .arg(uploaded.first).arg(formatSize(uploaded.second))
            .arg(m_ftpConnected ? "已连接" : "未连接")
    );
}

void MainWindow::updateDiskSpace() {
    Config& cfg = Config::instance();
    cfg.load();
    QString path = cfg.localPath();
    if (path.isEmpty()) path = "C:/";

    if (m_diskSpaceWidget) {
        m_diskSpaceWidget->updateFromPath(path);
    }

    if (m_diskSpaceLabel) {
        QString driveLetter = path.length() >= 2 && path[1] == ':' ? path.left(2) : "C:";
        m_diskSpaceLabel->setText("磁盘: " + driveLetter);
    }

    if (m_diskSpaceDetailLabel) {
        qint64 free = 0, total = 0;
        if (DiskTool::getDiskSpace(path, free, total)) {
            m_diskSpaceDetailLabel->setText(QString("共 %1 | 可用 %2")
                .arg(formatSize(total)).arg(formatSize(free)));
        } else {
            m_diskSpaceDetailLabel->setText("共 0 | 可用 0");
        }
    }
}

void MainWindow::resumePendingUploads() {
    Config& cfg = Config::instance();
    cfg.load();
    QString subPath = cfg.ftpConfig().value("sub_path").toString("/");
    QList<FileRecord> pending = FileRecordDB::instance().pendingRecords();
    if (pending.isEmpty()) return;
    m_logPanel->appendInfo(QString("恢复 %1 个待上传任务...").arg(pending.size()));
    for (const FileRecord& rec : pending) {
        m_uploadQueue->addFile(rec.id, rec.usbDrive, rec.filePath, rec.localPath, rec.fileSize);
        QString remotePath = subPath + "/" + rec.filePath;
        FTPUploadTask task;
        task.recordId = rec.id;
        task.localPath = rec.localPath;
        task.remotePath = remotePath;
        task.fileSize = rec.fileSize;
        task.status = "pending";
        m_ftpProcess->sendUpload(task);
        m_uploadQueue->updateFileStatus(rec.id, "uploading");
    }
    updateQueueCount();
}

QString MainWindow::formatSize(qint64 b) const {
    if (b >= 1024*1024*1024) return QString::number(b/1024.0/1024/1024,'f',1) + "GB";
    if (b >= 1024*1024) return QString::number(b/1024.0/1024,'f',1) + "MB";
    if (b >= 1024) return QString::number(b/1024.0,'f',1) + "KB";
    return QString::number(b) + "B";
}

void MainWindow::onAbout() {
    QMessageBox::about(this, "关于",
        "<html><body style='color:#374151; background:#F3F4F6; font-family: sans-serif;'>"
        "<h2>U盘自动转储工具</h2>"
        "<p>版本 1.0</p>"
        "<p>指挥中心 崔振东 制作</p>"
        "<p>有问题请联系：31990</p>"
        "<p style='color:#6B7280; margin-top:12px;'>"
        "功能：U盘自动转储 → 本地 → FTP上传 → 自动清理<br/>"
        "支持多USB并行转储，FTP目录结构保持"
        "</p>"
        "</body></html>"
    );
}
