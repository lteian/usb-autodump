#include "settings_dialog.h"
#include "config.h"
#include "crypto.h"
#include "file_record.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QHeaderView>
#include <QApplication>
#include <QStandardPaths>
#include <QRegExp>

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("⚙ 设置");
    setMinimumWidth(540);
    setStyleSheet(R"(
        QDialog { background: #1e1e1e; color: #e0e0e0; }
        QLabel { color: #e0e0e0; }
        QLineEdit { background: #2d2d2d; color: #e0e0e0; border: 1px solid #4a4a4a; padding: 6px; border-radius: 4px; }
        QCheckBox { color: #e0e0e0; }
        QSpinBox { background: #2d2d2d; color: #e0e0e0; border: 1px solid #4a4a4a; border-radius: 4px; }
        QTableWidget { background: #2d2d2d; color: #e0e0d4d4; gridline-color: #4a4a4a; }
        QHeaderView::section { background: #2d2d2d; color: #e0e0e0; border: 1px solid #4a4a4a; }
        QTabWidget::pane { border: 1px solid #4a4a4a; border-radius: 4px; }
        QTabBar::tab { background: #2d2d2d; color: #9e9e9e; padding: 6px 12px; }
        QTabBar::tab:selected { background: #424242; color: white; }
        QPushButton { background: #424242; color: white; border: none; border-radius: 4px; padding: 6px 12px; }
        QPushButton:hover { background: #616161; }
    )");

    QVBoxLayout* vl = new QVBoxLayout(this);

    // ── Encryption Warning ───────────────────────────────
    m_encWarn = new QLabel();
    m_encWarn->setVisible(false);
    m_encWarn->setStyleSheet("background: #FF9800; color: #1e1e1e; padding: 10px 14px; border-radius: 6px; font-weight: bold;");
    vl->addWidget(m_encWarn);

    // ── Encryption Password ────────────────────────────────
    QGroupBox* encGrp = new QGroupBox("🔐 加密密码");
    QVBoxLayout* encVl = new QVBoxLayout();

    QHBoxLayout* r1 = new QHBoxLayout();
    r1->addWidget(new QLabel("设置/修改密码:"));
    m_encPwdEdit = new QLineEdit();
    m_encPwdEdit->setEchoMode(QLineEdit::Password);
    m_encPwdEdit->setPlaceholderText("留空保持不变，输入则修改（至少4字符）");
    r1->addWidget(m_encPwdEdit);
    encVl->addLayout(r1);

    QHBoxLayout* r2 = new QHBoxLayout();
    r2->addWidget(new QLabel("确认密码:"));
    m_encPwd2Edit = new QLineEdit();
    m_encPwd2Edit->setEchoMode(QLineEdit::Password);
    r2->addWidget(m_encPwd2Edit);
    encVl->addLayout(r2);

    m_encError = new QLabel();
    m_encError->setStyleSheet("color: #F44336;");
    encVl->addWidget(m_encError);
    encGrp->setLayout(encVl);
    vl->addWidget(encGrp);

    // ── Tabs ─────────────────────────────────────────────
    m_tabs = new QTabWidget();

    // ── Local Tab ────────────────────────────────────────
    QWidget* localTab = new QWidget();
    QVBoxLayout* localVl = new QVBoxLayout(localTab);

    QHBoxLayout* lpRow = new QHBoxLayout();
    lpRow->addWidget(new QLabel("默认路径:"));
    m_localPathEdit = new QLineEdit();
    m_localPathEdit->setReadOnly(true);
    QPushButton* browseBtn = new QPushButton("浏览...");
    connect(browseBtn, &QPushButton::clicked, this, &SettingsDialog::onBrowseLocalPath);
    lpRow->addWidget(m_localPathEdit);
    lpRow->addWidget(browseBtn);
    localVl->addLayout(lpRow);

    localVl->addWidget(new QLabel("U盘专属路径（空则使用默认路径）:"));
    m_usbPathsTable = new QTableWidget();
    m_usbPathsTable->setColumnCount(2);
    m_usbPathsTable->setHorizontalHeaderLabels({"盘符", "本地路径"});
    m_usbPathsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_usbPathsTable->setRowCount(4);
    localVl->addWidget(m_usbPathsTable);
    localVl->addStretch();

    // ── FTP Tab ─────────────────────────────────────────
    QWidget* ftpTab = new QWidget();
    QFormLayout* ftpFl = new QFormLayout();

    m_ftpHost = new QLineEdit();
    m_ftpPort = new QSpinBox();
    m_ftpPort->setRange(1, 65535);
    m_ftpPort->setValue(21);
    m_ftpUser = new QLineEdit();
    m_ftpPass = new QLineEdit();
    m_ftpPass->setEchoMode(QLineEdit::Password);
    m_ftpPass->setPlaceholderText("留空则不修改当前密码");
    m_ftpSubPath = new QLineEdit("/");
    m_ftpTls = new QCheckBox("使用 TLS/SSL (FTPS)");

    ftpFl->addRow("服务器地址:", m_ftpHost);
    ftpFl->addRow("端口:", m_ftpPort);
    ftpFl->addRow("用户名:", m_ftpUser);
    ftpFl->addRow("密码:", m_ftpPass);
    ftpFl->addRow("子路径:", m_ftpSubPath);
    ftpFl->addRow("", m_ftpTls);
    ftpTab->setLayout(ftpFl);

    // ── Advanced Tab ─────────────────────────────────────
    QWidget* advTab = new QWidget();
    QVBoxLayout* advVl = new QVBoxLayout();
    m_autoFormat = new QCheckBox("复制完成后自动格式化U盘");
    m_autoDelete = new QCheckBox("上传后自动删除本地文件");
    m_retrySpin = new QSpinBox();
    m_retrySpin->setRange(1, 10);
    m_retrySpin->setValue(3);

    QHBoxLayout* retryRow = new QHBoxLayout();
    retryRow->addWidget(new QLabel("重试次数:"));
    retryRow->addWidget(m_retrySpin);
    retryRow->addStretch();

    advVl->addWidget(m_autoFormat);
    advVl->addWidget(m_autoDelete);
    advVl->addLayout(retryRow);
    advVl->addStretch();
    advTab->setLayout(advVl);

    m_tabs->addTab(localTab, "本地存储");
    m_tabs->addTab(ftpTab, "FTP 设置");
    m_tabs->addTab(advTab, "高级选项");
    vl->addWidget(m_tabs);

    // ── Buttons ──────────────────────────────────────────
    QHBoxLayout* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    QPushButton* cancelBtn = new QPushButton("取消");
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    QPushButton* saveBtn = new QPushButton("保存");
    saveBtn->setDefault(true);
    connect(saveBtn, &QPushButton::clicked, this, &SettingsDialog::onSave);
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(saveBtn);
    vl->addLayout(btnRow);

    loadCurrentConfig();
}

void SettingsDialog::loadCurrentConfig() {
    Config& cfg = Config::instance();
    cfg.load();

    if (!cfg.isPasswordSet()) {
        m_encWarn->setText("⚠️ 请先设置加密密码（用于加密 FTP 密码）");
        m_encWarn->setVisible(true);
    }

    m_localPathEdit->setText(cfg.localPath());

    QMap<QString, QString> usbPaths = cfg.usbPaths();
    int row = 0;
    for (auto it = usbPaths.constBegin(); it != usbPaths.constEnd() && row < 4; ++it, ++row) {
        m_usbPathsTable->setItem(row, 0, new QTableWidgetItem(it.key()));
        m_usbPathsTable->setItem(row, 1, new QTableWidgetItem(it.value()));
    }

    QJsonObject ftp = cfg.ftpConfig();
    m_ftpHost->setText(ftp.value("host").toString());
    m_ftpPort->setValue(ftp.value("port").toInt(21));
    m_ftpUser->setText(ftp.value("username").toString());
    m_ftpSubPath->setText(ftp.value("sub_path").toString("/"));
    m_ftpTls->setChecked(ftp.value("use_tls").toBool());

    m_autoFormat->setChecked(cfg.autoFormatAfterCopy());
    m_autoDelete->setChecked(cfg.autoDeleteLocal());
    m_retrySpin->setValue(cfg.maxRetry());
}

void SettingsDialog::onBrowseLocalPath() {
    QString dir = QFileDialog::getExistingDirectory(this, "选择默认本地存储路径",
                                                  m_localPathEdit->text());
    if (!dir.isEmpty()) m_localPathEdit->setText(dir);
}

void SettingsDialog::onSave() {
    // Validate encryption password
    QString newPwd = m_encPwdEdit->text();
    QString confirmPwd = m_encPwd2Edit->text();
    QString oldEncPwd = Config::instance().encryptionPassword();

    if (!newPwd.isEmpty()) {
        if (newPwd.length() < 4) {
            m_encError->setText("密码至少4个字符");
            return;
        }
        if (newPwd != confirmPwd) {
            m_encError->setText("两次输入的密码不一致");
            return;
        }
    }

    // Collect USB paths
    QMap<QString, QString> usbPaths;
    for (int r = 0; r < m_usbPathsTable->rowCount(); ++r) {
        QTableWidgetItem* di = m_usbPathsTable->item(r, 0);
        QTableWidgetItem* pi = m_usbPathsTable->item(r, 1);
        if (di && pi && !di->text().trimmed().isEmpty()) {
            QString drive = di->text().trimmed();
            if (!drive.endsWith(":")) drive += ":";
            usbPaths[drive] = pi->text().trimmed();
        }
    }

    // Encrypt FTP password if set
    QString ftpPass = m_ftpPass->text().trimmed();
    if (!ftpPass.isEmpty()) {
        if (newPwd.isEmpty() && oldEncPwd.isEmpty()) {
            m_encError->setText("请先设置加密密码才能保存 FTP 密码");
            return;
        }
        QString encPwd = newPwd.isEmpty() ? oldEncPwd : newPwd;
        ftpPass = Crypto::encrypt(ftpPass, encPwd);
        // Also update verification token
        if (!newPwd.isEmpty()) {
            QString token = Crypto::encrypt("USB_AUTO_DUMP_VERIFY_TOKEN_v1", newPwd);
            Config::instance().setPasswordVerificationToken(token);
        }
    }

    Config& cfg = Config::instance();

    if (!newPwd.isEmpty()) {
        cfg.setEncryptionPassword(newPwd);
    }

    QJsonObject ftp;
    ftp["host"] = m_ftpHost->text().trimmed();
    ftp["port"] = m_ftpPort->value();
    ftp["username"] = m_ftpUser->text().trimmed();
    if (!ftpPass.isEmpty()) {
        ftp["password"] = ftpPass;
    } else {
        ftp["password"] = cfg.ftpConfig().value("password"); // keep old
    }
    ftp["sub_path"] = m_ftpSubPath->text().trimmed().replace(QRegExp("/+$"), "");
    ftp["use_tls"] = m_ftpTls->isChecked();
    ftp["max_retry"] = m_retrySpin->value();

    // Build root object
    QJsonObject root;
    root["local_path"] = m_localPathEdit->text().trimmed();
    root["encryption_password"] = newPwd.isEmpty() ? oldEncPwd : newPwd;
    root["ftp"] = ftp;
    root["auto_delete_local"] = m_autoDelete->isChecked();
    root["auto_format_after_copy"] = m_autoFormat->isChecked();
    root["usb_paths"] = QJsonObject::fromVariantMap(usbPaths);

    // Keep video extensions
    QJsonArray exts;
    for (const QString& e : cfg.videoExtensions()) exts.append(e);
    root["video_extensions"] = exts;

    cfg.setData(root);

    QMessageBox::information(this, "保存成功", "配置已保存！");
    accept();
}
