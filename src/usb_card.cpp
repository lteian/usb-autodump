#include "usb_card.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>

USBCard::USBCard(QWidget* parent)
    : QFrame(parent)
{
    setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
    setStyleSheet(R"(
        QFrame {
            background: #2d2d2d;
            border: 1px solid #4a4a4a;
            border-radius: 8px;
            padding: 8px;
            min-width: 180px;
        }
    )");

    QVBoxLayout* vl = new QVBoxLayout(this);
    vl->setSpacing(4);

    // Drive letter + status
    QHBoxLayout* hl = new QHBoxLayout();
    m_driveLabel = new QLabel("空闲");
    m_driveLabel->setStyleSheet("color: #ffffff; font-size: 15px; font-weight: bold;");
    m_statusLabel = new QLabel("");
    m_statusLabel->setStyleSheet("color: #9e9e9e; font-size: 11px;");
    hl->addWidget(m_driveLabel);
    hl->addStretch();
    hl->addWidget(m_statusLabel);
    vl->addLayout(hl);

    // Size info row: total / used
    m_sizeLabel = new QLabel("");
    m_sizeLabel->setStyleSheet("color: #b0b0b0; font-size: 11px;");
    m_sizeLabel->setAlignment(Qt::AlignLeft);
    vl->addWidget(m_sizeLabel);

    // Free space label (new)
    m_freeSpaceLabel = new QLabel("");
    m_freeSpaceLabel->setStyleSheet("color: #b0b0b0; font-size: 11px;");
    m_freeSpaceLabel->setAlignment(Qt::AlignLeft);
    vl->addWidget(m_freeSpaceLabel);

    // Overall progress (outer: file count)
    m_overallProgress = new QProgressBar();
    m_overallProgress->setVisible(false);
    m_overallProgress->setStyleSheet(R"(
        QProgressBar {
            height: 10px;
            border-radius: 5px;
            background: #3c3c3c;
            text-align: center;
            color: transparent;
        }
        QProgressBar::chunk {
            background: #FF9800;
            border-radius: 5px;
        }
    )");
    vl->addWidget(m_overallProgress);

    // File progress (inner: current file %)
    m_fileProgress = new QProgressBar();
    m_fileProgress->setVisible(false);
    m_fileProgress->setStyleSheet(R"(
        QProgressBar {
            height: 8px;
            border-radius: 4px;
            background: #333333;
            text-align: center;
            color: transparent;
        }
        QProgressBar::chunk {
            background: #2196F3;
            border-radius: 4px;
        }
    )");
    vl->addWidget(m_fileProgress);

    // Speed + ETA row
    QHBoxLayout* speedRow = new QHBoxLayout();
    m_speedLabel = new QLabel("");
    m_speedLabel->setStyleSheet("color: #9e9e9e; font-size: 10px;");
    m_speedLabel->setVisible(false);
    m_etaLabel = new QLabel("");
    m_etaLabel->setStyleSheet("color: #9e9e9e; font-size: 10px;");
    m_etaLabel->setVisible(false);
    speedRow->addWidget(m_speedLabel);
    speedRow->addStretch();
    speedRow->addWidget(m_etaLabel);
    vl->addLayout(speedRow);

    // Current file name
    m_currentFileLabel = new QLabel("");
    m_currentFileLabel->setStyleSheet("color: #808080; font-size: 10px;");
    m_currentFileLabel->setAlignment(Qt::AlignLeft);
    m_currentFileLabel->setVisible(false);
    m_currentFileLabel->setWordWrap(true);
    vl->addWidget(m_currentFileLabel);

    // Upload status row
    QHBoxLayout* uploadRow = new QHBoxLayout();
    m_uploadStatusLabel = new QLabel("");
    m_uploadStatusLabel->setStyleSheet("color: #9e9e9e; font-size: 11px;");
    m_uploadStatusLabel->setVisible(false);
    uploadRow->addWidget(m_uploadStatusLabel);
    vl->addLayout(uploadRow);

    m_uploadProgress = new QProgressBar();
    m_uploadProgress->setVisible(false);
    m_uploadProgress->setStyleSheet(R"(
        QProgressBar {
            height: 6px;
            border-radius: 3px;
            background: #3c3c3c;
            text-align: center;
            color: transparent;
        }
        QProgressBar::chunk {
            background: #4CAF50;
            border-radius: 3px;
        }
    )");
    vl->addWidget(m_uploadProgress);

    vl->addStretch();

    // Buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();

    m_formatBtn = new QPushButton("格式化");
    m_formatBtn->setVisible(false);
    m_formatBtn->setStyleSheet("QPushButton { background: #F44336; color: white; border: none; border-radius: 4px; padding: 4px 8px; font-size: 12px; } QPushButton:hover { background: #e53935; }");
    connect(m_formatBtn, &QPushButton::clicked, this, [this](){ emit formatClicked(m_drive); });

    m_ejectBtn = new QPushButton("弹出");
    m_ejectBtn->setVisible(false);
    m_ejectBtn->setStyleSheet("QPushButton { background: #616161; color: white; border: none; border-radius: 4px; padding: 4px 8px; font-size: 12px; } QPushButton:hover { background: #757575; }");
    connect(m_ejectBtn, &QPushButton::clicked, this, [this](){ emit ejectClicked(m_drive); });

    m_cancelBtn = new QPushButton("取消");
    m_cancelBtn->setVisible(false);
    m_cancelBtn->setStyleSheet("QPushButton { background: #757575; color: white; border: none; border-radius: 4px; padding: 4px 8px; font-size: 12px; } QPushButton:hover { background: #9e9e9e; }");
    connect(m_cancelBtn, &QPushButton::clicked, this, [this](){ emit cancelDumpClicked(m_drive); });

    btnLayout->addWidget(m_formatBtn);
    btnLayout->addWidget(m_ejectBtn);
    btnLayout->addWidget(m_cancelBtn);
    vl->addLayout(btnLayout);
}

USBCard::~USBCard() {}

void USBCard::setDrive(const QString& letter, const QString& label,
                      qint64 total, qint64 used, qint64 freeSpace) {
    m_drive = letter;
    QString labelStr = label.isEmpty() ? "" : (" " + label);
    m_driveLabel->setText(letter + labelStr);

    // Size info
    m_sizeLabel->setText(fmtSize(used) + " / " + fmtSize(total));

    // Free space with color warning
    double freePct = total > 0 ? (freeSpace * 100.0 / total) : 0;
    QString freeColor = "#b0b0b0";
    if (freePct < 5) freeColor = "#F44336";
    else if (freePct < 10) freeColor = "#FF9800";
    m_freeSpaceLabel->setText(QString("剩余: <span style='color:%1'>%2</span>").arg(freeColor).arg(fmtSize(freeSpace)));
    m_freeSpaceLabel->setVisible(true);

    setStatus("idle");
}

void USBCard::setStatus(const QString& s) {
    m_progress = false;
    m_cancelBtn->setVisible(false);
    m_formatBtn->setVisible(false);
    m_ejectBtn->setVisible(false);
    m_overallProgress->setVisible(false);
    m_fileProgress->setVisible(false);
    m_speedLabel->setVisible(false);
    m_etaLabel->setVisible(false);
    m_currentFileLabel->setVisible(false);
    m_uploadStatusLabel->setVisible(false);
    m_uploadProgress->setVisible(false);

    if (s == "idle") {
        m_statusLabel->setText("空闲");
        m_statusLabel->setStyleSheet("color: #9e9e9e; font-size: 11px;");
    } else if (s == "copying") {
        m_statusLabel->setText("复制中...");
        m_statusLabel->setStyleSheet("color: #FF9800; font-size: 11px;");
        m_overallProgress->setVisible(true);
        m_fileProgress->setVisible(true);
        m_speedLabel->setVisible(true);
        m_etaLabel->setVisible(true);
        m_currentFileLabel->setVisible(true);
        m_cancelBtn->setVisible(true);
    } else if (s == "done") {
        m_statusLabel->setText("待操作");
        m_statusLabel->setStyleSheet("color: #4CAF50; font-size: 11px;");
        m_formatBtn->setVisible(true);
        m_ejectBtn->setVisible(true);
    } else if (s == "formatting") {
        m_statusLabel->setText("格式化中...");
        m_statusLabel->setStyleSheet("color: #F44336; font-size: 11px;");
    }
}

void USBCard::updateProgress(int done, int total,
                              double fileProgress,
                              double speedMBps,
                              int etaSeconds,
                              const QString& currentFile) {
    m_overallProgress->setMaximum(total);
    m_overallProgress->setValue(done);
    m_overallProgress->setVisible(true);

    m_fileProgress->setMaximum(100);
    m_fileProgress->setValue(int(fileProgress));
    m_fileProgress->setVisible(true);

    if (speedMBps > 0) {
        m_speedLabel->setText(QString::number(speedMBps, 'f', 1) + " MB/s");
        m_speedLabel->setVisible(true);
    }

    if (etaSeconds > 0) {
        int mins = etaSeconds / 60;
        int secs = etaSeconds % 60;
        m_etaLabel->setText(QString("剩余 %1:%2").arg(mins, 2, 10, QChar('0')).arg(secs, 2, 10, QChar('0')));
        m_etaLabel->setVisible(true);
    }

    // Current file shown on the card directly
    m_currentFileLabel->setText(currentFile);
    m_currentFileLabel->setToolTip(currentFile);
    m_currentFileLabel->setVisible(true);
}

void USBCard::setUploadStatus(const QString& status) {
    if (status == "uploading") {
        m_uploadStatusLabel->setText("FTP: 上传中...");
        m_uploadStatusLabel->setStyleSheet("color: #2196F3; font-size: 11px;");
        m_uploadStatusLabel->setVisible(true);
        m_uploadProgress->setVisible(true);
    } else if (status == "uploaded") {
        m_uploadStatusLabel->setText("FTP: ✓已上传");
        m_uploadStatusLabel->setStyleSheet("color: #4CAF50; font-size: 11px;");
        m_uploadStatusLabel->setVisible(true);
        m_uploadProgress->setVisible(false);
    } else if (status == "error") {
        m_uploadStatusLabel->setText("FTP: ✗失败");
        m_uploadStatusLabel->setStyleSheet("color: #F44336; font-size: 11px;");
        m_uploadStatusLabel->setVisible(true);
        m_uploadProgress->setVisible(false);
    } else {
        m_uploadStatusLabel->setVisible(false);
        m_uploadProgress->setVisible(false);
    }
}

void USBCard::setUploadProgress(double pct) {
    m_uploadProgress->setMaximum(100);
    m_uploadProgress->setValue(int(pct));
}

void USBCard::clear() {
    m_drive.clear();
    m_driveLabel->setText("空闲");
    m_statusLabel->setText("");
    m_sizeLabel->setText("");
    m_freeSpaceLabel->setVisible(false);
    m_overallProgress->setVisible(false);
    m_overallProgress->setValue(0);
    m_fileProgress->setVisible(false);
    m_fileProgress->setValue(0);
    m_speedLabel->setVisible(false);
    m_etaLabel->setVisible(false);
    m_currentFileLabel->setVisible(false);
    m_currentFileLabel->setText("");
    m_uploadStatusLabel->setVisible(false);
    m_uploadProgress->setVisible(false);
    m_formatBtn->setVisible(false);
    m_ejectBtn->setVisible(false);
    m_cancelBtn->setVisible(false);
}

QString USBCard::fmtSize(qint64 b) {
    if (b >= 1024*1024*1024) return QString::number(b/1024.0/1024/1024, 'f', 1) + "GB";
    if (b >= 1024*1024) return QString::number(b/1024.0/1024, 'f', 1) + "MB";
    if (b >= 1024) return QString::number(b/1024.0, 'f', 1) + "KB";
    return QString::number(b) + "B";
}
