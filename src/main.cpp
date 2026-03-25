#include "mainwindow.h"
#include "usb_card.h"
#include "log_panel.h"
#include "settings_dialog.h"
#include "usb_monitor.h"
#include "copy_engine.h"
#include "ftp_uploader.h"
#include "disk_tool.h"
#include "file_record.h"
#include "config.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QStatusBar>
#include <QTableWidget>
#include <QHeaderView>
#include <QScrollArea>
#include <QDebug>
#include <QLineEdit>
#include <QInputDialog>
#include <QMessageBox>
#include <QGroupBox>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("🚀 U盘自动转储工具");
    setMinimumSize(900, 600);
    setStyleSheet("QMainWindow { background: #1e1e1e; } QLabel { color: #e0e0e0; } QStatusBar { background: #252525; color: #b0b0b0; }");

    // Central widget
    QWidget* central = new QWidget();
    setCentralWidget(central);
    QVBoxLayout* vl = new QVBoxLayout(central);

    // ── Header ────────────────────────────────────────────
    QFrame* header = new QFrame();
    header->setStyleSheet("background: #252525; border-bottom: 1px solid #3c3c3c; padding: 8px;");
    QHBoxLayout* hl = new QHBoxLayout(header);

    QLabel* title = new QLabel("🚀 U盘自动转储工具");
    title->setStyleSheet("font-size: 16px; font-weight: bold; color: #ffffff;");
    hl->addWidget(title);
    hl->addStretch();

    QPushButton* settingsBtn = new QPushButton("⚙ 设置");
    settingsBtn->setStyleSheet("background: #424242; color: white; border: none; border-radius: 4px; padding: 6px 12px;");
    connect(settingsBtn, &QPushButton::clicked, this, &MainWindow::onSettingsClicked);
    hl->addWidget(settingsBtn);

    m_ftpStatusLabel = new QLabel("FTP: ○未连接");
    m_ftpStatusLabel->setStyleSheet("color: #9e9e9e; font-size: 12px; padding: 0 8px;");
    hl->addWidget(m_ftpStatusLabel);
    vl->addWidget(header);

    // ── USB Cards Area ───────────────────────────────────
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
        connect(card, &USBCard::cancelClicked, this, &MainWindow::onCancelClicked);
    }
    scroll->setWidget(cardsWidget);
    vl->addWidget(scroll, 1);

    // ── Pending Files List ────────────────────────────────
    QGroupBox* pendingGrp = new QGroupBox("📋 待上传文件");
    pendingGrp->setStyleSheet(R"(
        QGroupBox { color: #e0e0e0; border: 1px solid #4a4a4a; border-radius: 4px; margin-top: 8px; }
        QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }
    )");
    QVBoxLayout* pendingVl = new QVBoxLayout(pendingGrp);
    m_pendingList = new QTableWidget();
    m_pendingList->setColumnCount(4);
    m_pendingList->setHorizontalHeaderLabels({"盘符", "文件名", "大小", "状态"});
    m_pendingList->setColumnWidth(0, 60);
    m_pendingList->setColumnWidth(1, 400);
    m_pendingList->setColumnWidth(2, 100);
    m_pendingList->setColumnWidth(3, 80);
    m_pendingList->setMaximumHeight(120);
    m_pendingList->setStyleSheet(R"(
        QTableWidget { background: #2d2d2d; color: #d4d4d4; gridline-color: #4a4a4a; }
        QHeaderView::section { background: #2d2d2d; color: #d4d4d4; border: 1px solid #4a4a4a; }
    )");
    pendingVl->addWidget(m_pendingList);
    vl->addWidget(pendingGrp);

    // ── Log Panel ────────────────────────────────────────
    m_logPanel = new LogPanel();
    vl->addWidget(m_logPanel);

    // ── Status Bar ────────────────────────────────────────
    QStatusBar* sb = new QStatusBar();
    setStatusBar(sb);

    // ── Services ────────────────────────────────────────────
    m_usbMonitor = new USBMonitor(this);
    m_copyEngine = new CopyEngine(this);
    m_ftpUploader = new FTPUploader(this);

    connect(m_usbMonitor, &USBMonitor::deviceInserted, this, &MainWindow::onDeviceInserted);
    connect(m_usbMonitor, &USBMonitor::deviceRemoved, this, &MainWindow::onDeviceRemoved);
    connect(m_copyEngine, &CopyEngine::copyProgress, this, &MainWindow::onCopyProgress);
    connect(m_copyEngine, &CopyEngine::copyDone, this, &MainWindow::onCopyDone);
    connect(m_ftpUploader, &FTPUploader::uploadDone, this, &MainWindow::onUploadDone);
    connect(m_ftpUploader, &FTPUploader::connectedChanged, this, [this](bool connected) {
        m_ftpStatusLabel->setText(connected ? "FTP: ●已连接" : "FTP: ○未连接");
        m_ftpStatusLabel->setStyleSheet(connected
            ? "color: #4CAF50; font-size: 12px; padding: 0 8px;"
            : "color: #9e9e9e; font-size: 12px; padding: 0 8px;");
    });
    connect(m_ftpUploader, &FTPUploader::logMessage, m_logPanel, &LogPanel::appendInfo);

    // Start services
    m_ftpUploader->start();

    // Initial device scan
    for (const USBDevice& dev : m_usbMonitor->currentDevices()) {
        QTimer::singleShot(500, this, [this, dev]() {
            onDeviceInserted(dev);
        });
    }

    // Status bar timer
    QTimer* statusTimer = new QTimer(this);
    connect(statusTimer, &QTimer::timeout, this, &MainWindow::updateStatusBar);
    statusTimer->start(2000);

    m_logPanel->appendInfo("服务已启动，等待 USB 设备...");
}

MainWindow::~MainWindow() {
    m_ftpUploader->stop();
}

void MainWindow::onDeviceInserted(const USBDevice& dev) {
    QString drive = dev.driveLetter;
    m_logPanel->appendInfo(QString("%1 插入，容量 %2GB，已用 %3%")
        .arg(drive)
        .arg(dev.totalSize / 1024.0 / 1024 / 1024, 0, 'f', 1)
        .arg(int(dev.usedPercent())));
    allocateCard(drive, dev);
    onSettingsClicked(); // force settings check on first insertion
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
            card->setDrive(drive, dev.label, dev.totalSize, dev.usedSize());
            card->setStatus("copying");
            m_copyEngine->startCopy(drive);
            return;
        }
    }
}

void MainWindow::releaseCard(const QString& drive) {
    USBCard* card = m_driveToCard.take(drive);
    if (card) {
        card->clear();
        card->setProperty("drive", QString());
    }
}

void MainWindow::onCopyProgress(const QString& drive, int done, int total, const CopyTask& task) {
    USBCard* card = m_driveToCard.value(drive, nullptr);
    if (card) {
        card->updateProgress(done, total, task.progress);
        // Show current file name
        QString fname = task.srcPath;
        int p = fname.lastIndexOf('/');
        if (p < 0) p = fname.lastIndexOf('\\');
        if (p >= 0) fname = fname.mid(p + 1);
        card->setCurrentFile(fname);
        if (task.status == "copied") {
            m_logPanel->appendInfo("复制完成: " + task.srcPath);
        }
    }
}

void MainWindow::onCopyDone(const QString& drive, const QList<CopyTask>& tasks) {
    // Refresh pending list with all copied files
    refreshPendingList();
    USBCard* card = m_driveToCard.value(drive, nullptr);
    if (!card) return;

    int copied = 0, errors = 0;
    for (const CopyTask& t : tasks) {
        if (t.status == "copied") copied++;
        else if (t.status == "error") errors++;
    }
    m_logPanel->appendInfo(QString("%1 复制完成: %2个成功，%3个失败").arg(drive).arg(copied).arg(errors));
    card->setStatus("done");
    updateStatusBar();
}

void MainWindow::onUploadDone(const UploadTask& task) {
    refreshPendingList();
    if (task.status == "uploaded") {
        m_logPanel->appendInfo("上传成功: " + task.localPath);
    } else if (task.status == "error") {
        m_logPanel->appendError("上传失败: " + task.localPath + " - " + task.errorMsg);
    }
    updateStatusBar();
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

void MainWindow::onCancelClicked(const QString& drive) {
    m_copyEngine->cancelCopy(drive);
    m_logPanel->appendWarning("已取消复制: " + drive);
    USBCard* card = m_driveToCard.value(drive, nullptr);
    if (card) card->setStatus("idle");
}

void MainWindow::onSettingsClicked() {
    // Check if password is set
    Config& cfg = Config::instance();
    cfg.load();
    if (!cfg.isPasswordSet()) {
        QMessageBox::warning(this, "请先设置密码",
            "请先设置加密密码，再配置 FTP。\n\n"
            "打开「设置 ⚙」→「加密密码」输入密码后保存。");
    }
    SettingsDialog dlg(this);
    dlg.exec();
}

void MainWindow::refreshPendingList() {
    if (!m_pendingList) return;
    Config& cfg = Config::instance();
    cfg.load();

    m_pendingList->setRowCount(0);
    QList<FileRecord> pending = FileRecordDB::instance().pendingRecords();

    for (const FileRecord& r : pending) {
        int row = m_pendingList->rowCount();
        m_pendingList->insertRow(row);

        // Drive
        QTableWidgetItem* driveItem = new QTableWidgetItem(r.usbDrive);
        driveItem->setForeground(QColor("#FF9800"));
        m_pendingList->setItem(row, 0, driveItem);

        // File name (extract just filename from path)
        QString fname = r.filePath;
        int slashPos = fname.lastIndexOf('/');
        if (slashPos < 0) slashPos = fname.lastIndexOf('\\');
        if (slashPos >= 0) fname = fname.mid(slashPos + 1);
        QTableWidgetItem* nameItem = new QTableWidgetItem(fname);
        nameItem->setForeground(QColor("#d4d4d4"));
        m_pendingList->setItem(row, 1, nameItem);

        // Size
        QString sizeStr;
        qint64 sz = r.fileSize;
        if (sz >= 1024*1024*1024) sizeStr = QString::number(sz/1024.0/1024/1024,'f',1)+"GB";
        else if (sz >= 1024*1024) sizeStr = QString::number(sz/1024.0/1024,'f',1)+"MB";
        else if (sz >= 1024) sizeStr = QString::number(sz/1024.0,'f',1)+"KB";
        else sizeStr = QString::number(sz)+"B";
        m_pendingList->setItem(row, 2, new QTableWidgetItem(sizeStr));

        // Status
        QString statusText;
        QColor statusColor;
        if (r.status == "pending") { statusText = "待上传"; statusColor = QColor("#FF9800"); }
        else if (r.status == "copying") { statusText = "复制中"; statusColor = QColor("#2196F3"); }
        else if (r.status == "copied") { statusText = "已复制"; statusColor = QColor("#4CAF50"); }
        else if (r.status == "uploading") { statusText = "上传中"; statusColor = QColor("#2196F3"); }
        else if (r.status == "uploaded") { statusText = "已上传"; statusColor = QColor("#4CAF50"); }
        else if (r.status == "error") { statusText = "错误"; statusColor = QColor("#F44336"); }
        else { statusText = r.status; statusColor = QColor("#9e9e9e"); }
        QTableWidgetItem* statusItem = new QTableWidgetItem(statusText);
        statusItem->setForeground(statusColor);
        m_pendingList->setItem(row, 3, statusItem);
    }
}

QString MainWindow::formatSize(qint64 b) {
    if (b >= 1024*1024*1024) return QString::number(b/1024.0/1024/1024,'f',1) + "GB";
    if (b >= 1024*1024) return QString::number(b/1024.0/1024,'f',1) + "MB";
    if (b >= 1024) return QString::number(b/1024.0,'f',1) + "KB";
    return QString::number(b) + "B";
}

void MainWindow::updateStatusBar() {
    // Only refresh pending list every 10 seconds to avoid DB overhead
    static int tick = 0;
    if (++tick % 5 == 0) {
        refreshPendingList();
    }
    auto pending = FileRecordDB::instance().pendingCountAndSize();
    auto uploaded = FileRecordDB::instance().uploadedCountAndSize();
    int queue = m_ftpUploader ? m_ftpUploader->queueSize() : 0;

    statusBar()->showMessage(
        QString("  待上传: %1个 (%2)  |  已上传: %3个 (%4)  |  队列: %5")
            .arg(pending.first).arg(formatSize(pending.second))
            .arg(uploaded.first).arg(formatSize(uploaded.second))
            .arg(queue)
    );
}
