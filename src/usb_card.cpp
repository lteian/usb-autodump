#include "usb_card.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>

USBCard::USBCard(QWidget* parent)
    : QFrame(parent)
{
    setFrameStyle(QFrame::NoFrame);
    setMinimumWidth(100);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setStyleSheet(R"(
        QFrame {
            background: #F9FAFB;
            border: 1px solid #E5E6EB;
            border-radius: 8px;
        }
    )");

    QVBoxLayout* vl = new QVBoxLayout(this);
    vl->setSpacing(4);
    vl->setContentsMargins(10, 8, 10, 8);

    // Drive + status row
    QHBoxLayout* topRow = new QHBoxLayout();
    topRow->setSpacing(4);
    m_driveLabel = new QLabel("空闲");
    m_driveLabel->setStyleSheet("color: #86909C; font-size: 13px; font-weight: 600; font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif;");
    m_statusLabel = new QLabel("");
    m_statusLabel->setStyleSheet("color: #86909C; font-size: 10px; font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif;");
    topRow->addWidget(m_driveLabel, 0, Qt::AlignLeft);
    topRow->addWidget(m_statusLabel, 1, Qt::AlignRight);
    vl->addLayout(topRow);

    // Hint (idle state)
    m_hintLabel = new QLabel("空闲");
    m_hintLabel->setStyleSheet("color: #86909C; font-size: 11px; font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif;");
    m_hintLabel->setAlignment(Qt::AlignCenter);
    vl->addWidget(m_hintLabel);

    // Size info
    m_sizeLabel = new QLabel("");
    m_sizeLabel->setStyleSheet("color: #4E5969; font-size: 11px; font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif;");
    m_sizeLabel->setAlignment(Qt::AlignCenter);
    vl->addWidget(m_sizeLabel);

    vl->addStretch();

    // Progress bar (hidden by default)
    m_overallProgress = new QProgressBar();
    m_overallProgress->setVisible(false);
    m_overallProgress->setStyleSheet(R"(
        QProgressBar {
            height: 4px;
            border-radius: 2px;
            background: rgba(0,0,0,0.08);
            text-align: center;
        }
        QProgressBar::chunk {
            background: #165DFF;
            border-radius: 2px;
        }
    )");
    vl->addWidget(m_overallProgress);

    // Speed + ETA
    QHBoxLayout* speedRow = new QHBoxLayout();
    speedRow->setSpacing(4);
    m_speedLabel = new QLabel("");
    m_speedLabel->setStyleSheet("color: #86909C; font-size: 10px; font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif;");
    m_speedLabel->setVisible(false);
    m_etaLabel = new QLabel("");
    m_etaLabel->setStyleSheet("color: #86909C; font-size: 10px; font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif;");
    m_etaLabel->setVisible(false);
    speedRow->addWidget(m_speedLabel);
    speedRow->addStretch();
    speedRow->addWidget(m_etaLabel);
    vl->addLayout(speedRow);

    // Current file
    m_currentFileLabel = new QLabel("");
    m_currentFileLabel->setStyleSheet("color: #86909C; font-size: 9px; font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif;");
    m_currentFileLabel->setVisible(false);
    m_currentFileLabel->setWordWrap(true);
    vl->addWidget(m_currentFileLabel);

    // Buttons row
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(6);

    m_formatBtn = new QPushButton("格式化");
    m_formatBtn->setVisible(false);
    m_formatBtn->setStyleSheet(R"(
        QPushButton {
            background: #FEE2E2;
            color: #F53F3F;
            border: none;
            border-radius: 4px;
            padding: 3px 8px;
            font-size: 11px;
            font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif;
        }
        QPushButton:hover { background: #FECACA; }
    )");
    connect(m_formatBtn, &QPushButton::clicked, this, [this](){ emit formatClicked(m_drive); });

    m_ejectBtn = new QPushButton("弹出");
    m_ejectBtn->setVisible(false);
    m_ejectBtn->setStyleSheet(R"(
        QPushButton {
            background: #F3F4F6;
            color: #4E5969;
            border: none;
            border-radius: 4px;
            padding: 3px 8px;
            font-size: 11px;
            font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif;
        }
        QPushButton:hover { background: #E5E7EB; }
    )");
    connect(m_ejectBtn, &QPushButton::clicked, this, [this](){ emit ejectClicked(m_drive); });

    m_cancelBtn = new QPushButton("取消");
    m_cancelBtn->setVisible(false);
    m_cancelBtn->setStyleSheet(R"(
        QPushButton {
            background: #F3F4F6;
            color: #86909C;
            border: none;
            border-radius: 4px;
            padding: 3px 8px;
            font-size: 11px;
            font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif;
        }
        QPushButton:hover { background: #E5E7EB; }
    )");
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
    m_driveLabel->setStyleSheet("color: #165DFF; font-size: 13px; font-weight: 700; font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif;");

    // Blue background for USB, gray for local drive
    bool isLocal = label.toUpper().contains("SSD") || label.toUpper().contains("本地") || label.toUpper().contains("HDD");
    if (isLocal) {
        setStyleSheet(R"(
            QFrame {
                background: #F5F7FA;
                border: 1px solid #E5E6EB;
                border-radius: 8px;
            }
        )");
    } else {
        setStyleSheet(R"(
            QFrame {
                background: #E8F3FF;
                border: 1px solid #C6DCFF;
                border-radius: 8px;
            }
        )");
    }

    m_sizeLabel->setText(fmtSize(total));
    m_sizeLabel->setAlignment(Qt::AlignCenter);
    setStatus("idle");
}

void USBCard::setStatus(const QString& s) {
    m_cancelBtn->setVisible(false);
    m_formatBtn->setVisible(false);
    m_ejectBtn->setVisible(false);
    m_overallProgress->setVisible(false);
    m_speedLabel->setVisible(false);
    m_etaLabel->setVisible(false);
    m_currentFileLabel->setVisible(false);
    m_hintLabel->setVisible(false);

    if (s == "idle") {
        m_statusLabel->setText("");
        m_hintLabel->setVisible(true);
    } else if (s == "copying") {
        m_statusLabel->setText("复制中");
        m_statusLabel->setStyleSheet("color: #165DFF; font-size: 10px; font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif;");
        m_overallProgress->setVisible(true);
        m_speedLabel->setVisible(true);
        m_etaLabel->setVisible(true);
        m_currentFileLabel->setVisible(true);
        m_cancelBtn->setVisible(true);
    } else if (s == "done") {
        m_statusLabel->setText("已完成");
        m_statusLabel->setStyleSheet("color: #00B42A; font-size: 10px; font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif; font-weight: 500;");
        m_formatBtn->setVisible(true);
        m_ejectBtn->setVisible(true);
    } else if (s == "formatting") {
        m_statusLabel->setText("格式化中");
        m_statusLabel->setStyleSheet("color: #FF7D00; font-size: 10px; font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif; font-weight: 500;");
    }
}

void USBCard::updateProgress(int done, int total,
                              double,
                              double speedMBps,
                              int etaSeconds,
                              const QString& currentFile) {
    m_overallProgress->setMaximum(total);
    m_overallProgress->setValue(done);

    if (speedMBps > 0) {
        m_speedLabel->setText(QString::number(speedMBps, 'f', 1) + "MB/s");
        m_speedLabel->setVisible(true);
    }

    if (etaSeconds > 0) {
        int mins = etaSeconds / 60;
        int secs = etaSeconds % 60;
        m_etaLabel->setText(QString("%1:%2").arg(mins, 2, 10, QChar('0')).arg(secs, 2, 10, QChar('0')));
        m_etaLabel->setVisible(true);
    }

    m_currentFileLabel->setText(currentFile);
    m_currentFileLabel->setToolTip(currentFile);
    m_currentFileLabel->setVisible(true);
}

void USBCard::clear() {
    m_drive.clear();
    m_driveLabel->setText("空闲");
    m_driveLabel->setStyleSheet("color: #86909C; font-size: 13px; font-weight: 600; font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif;");
    m_statusLabel->setText("");
    m_sizeLabel->setText("");
    m_hintLabel->setVisible(true);
    setStyleSheet(R"(
        QFrame {
            background: #F9FAFB;
            border: 1px solid #E5E6EB;
            border-radius: 8px;
        }
    )");
    m_overallProgress->setVisible(false);
    m_overallProgress->setValue(0);
    m_speedLabel->setVisible(false);
    m_etaLabel->setVisible(false);
    m_currentFileLabel->setVisible(false);
    m_currentFileLabel->setText("");
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
