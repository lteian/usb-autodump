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
#include <QRegularExpression>
#include <QGroupBox>
#include <QTcpSocket>
#include <QTimer>
#include <QEventLoop>
#include <QThread>
#include <QJsonArray>

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("设置");
    setMinimumWidth(540);
    setStyleSheet(R"(
        QDialog { background: #F3F4F6; color: #374151; }
        QLabel { color: #374151; }
        QLineEdit {
            background: #FFFFFF;
            color: #374151;
            border: 1px solid #E5E7EB;
            padding: 6px;
            border-radius: 6px;
        }
        QLineEdit:focus { border-color: #3B82F6; }
        QCheckBox { color: #374151; }
        QSpinBox {
            background: #FFFFFF;
            color: #374151;
            border: 1px solid #E5E7EB;
            border-radius: 6px;
            padding: 4px;
        }
        QTableWidget {
            background: #FFFFFF;
            color: #374151;
            gridline-color: #E5E7EB;
            border: 1px solid #E5E7EB;
            border-radius: 8px;
        }
        QHeaderView::section {
            background: #F9FAFB;
            color: #6B7280;
            border: none;
            border-bottom: 1px solid #E5E7EB;
            padding: 6px;
        }
        QTabWidget::pane {
            border: 1px solid #E5E7EB;
            border-radius: 8px;
            background: #FFFFFF;
        }
        QTabBar::tab {
            background: #F3F4F6;
            color: #6B7280;
            padding: 8px 16px;
            border-top-left-radius: 6px;
            border-top-right-radius: 6px;
        }
        QTabBar::tab:selected {
            background: #FFFFFF;
            color: #374151;
            font-weight: 500;
        }
        QPushButton {
            background: #FFFFFF;
            color: #374151;
            border: 1px solid #E5E7EB;
            border-radius: 6px;
            padding: 6px 14px;
            font-size: 13px;
        }
        QPushButton:hover { background: #F9FAFB; }
    )");

    QVBoxLayout* vl = new QVBoxLayout(this);

    // ── Encryption Warning ───────────────────────────────
    m_encWarn = new QLabel();
    m_encWarn->setVisible(false);
    m_encWarn->setStyleSheet("background: #FEF3C7; color: #92400E; padding: 10px 14px; border-radius: 6px; font-weight: 500;");
    vl->addWidget(m_encWarn);

    // ── Encryption Password ────────────────────────────────
    QGroupBox* encGrp = new QGroupBox("加密密码");
    encGrp->setStyleSheet(R"(
        QGroupBox {
            background: #FFFFFF;
            border: 1px solid #E5E7EB;
            border-radius: 8px;
            padding: 16px;
            margin-top: 4px;
            font-weight: 600;
            color: #374151;
        }
        QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }
    )");
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
    m_encError->setStyleSheet("color: #EF4444;");
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
    browseBtn->setStyleSheet(R"(
        QPushButton { background: #3B82F6; color: white; border: none; border-radius: 6px; padding: 6px 14px; }
        QPushButton:hover { background: #2563EB; }
    )");
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
    m_ftpEncoding = new QComboBox();
    m_ftpEncoding->addItem("UTF-8", "utf8");
    m_ftpEncoding->addItem("GBK (中文服务器)", "gbk");

    ftpFl->addRow("服务器地址:", m_ftpHost);
    ftpFl->addRow("端口:", m_ftpPort);
    ftpFl->addRow("用户名:", m_ftpUser);
    ftpFl->addRow("密码:", m_ftpPass);
    ftpFl->addRow("子路径:", m_ftpSubPath);
    ftpFl->addRow("", m_ftpTls);
    ftpFl->addRow("路径编码:", m_ftpEncoding);

    QPushButton* testBtn = new QPushButton("测试FTP连接");
    testBtn->setStyleSheet(R"(
        QPushButton { background: #3B82F6; color: white; border: none; border-radius: 6px; padding: 8px 16px; font-size: 13px; }
        QPushButton:hover { background: #2563EB; }
    )");
    connect(testBtn, &QPushButton::clicked, this, &SettingsDialog::onTestFTP);
    ftpFl->addRow("", testBtn);

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
    saveBtn->setStyleSheet(R"(
        QPushButton { background: #3B82F6; color: white; border: none; border-radius: 6px; padding: 6px 16px; font-size: 13px; }
        QPushButton:hover { background: #2563EB; }
    )");
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
        m_encWarn->setText("请先设置加密密码（用于加密 FTP 密码）");
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
    int idx = m_ftpEncoding->findData(ftp.value("encoding").toString("utf8"));
    if (idx >= 0) m_ftpEncoding->setCurrentIndex(idx);

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

    QString ftpPass = m_ftpPass->text().trimmed();
    if (!ftpPass.isEmpty()) {
        if (newPwd.isEmpty() && oldEncPwd.isEmpty()) {
            m_encError->setText("请先设置加密密码才能保存 FTP 密码");
            return;
        }
        QString encPwd = newPwd.isEmpty() ? oldEncPwd : newPwd;
        ftpPass = Crypto::encrypt(ftpPass, encPwd);
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
        ftp["password"] = cfg.ftpConfig().value("password");
    }
    ftp["sub_path"] = m_ftpSubPath->text().trimmed().replace(QRegularExpression("/+$"), "");
    ftp["use_tls"] = m_ftpTls->isChecked();
    ftp["encoding"] = m_ftpEncoding->currentData().toString();
    ftp["max_retry"] = m_retrySpin->value();

    QJsonObject root;
    root["local_path"] = m_localPathEdit->text().trimmed();
    root["encryption_password"] = newPwd.isEmpty() ? oldEncPwd : newPwd;
    root["ftp"] = ftp;
    root["auto_delete_local"] = m_autoDelete->isChecked();
    root["auto_format_after_copy"] = m_autoFormat->isChecked();
    QVariantMap vmUsbPaths;
    for (auto it = usbPaths.constBegin(); it != usbPaths.constEnd(); ++it) {
        vmUsbPaths[it.key()] = it.value();
    }
    root["usb_paths"] = QJsonObject::fromVariantMap(vmUsbPaths);

    QJsonArray exts;
    for (const QString& e : cfg.videoExtensions()) exts.append(e);
    root["video_extensions"] = exts;

    cfg.setData(root);

    QMessageBox::information(this, "保存成功", "配置已保存！");
    accept();
}

void SettingsDialog::onResetAll() {
    reject();
}

void SettingsDialog::onTestFTP() {
    QString host = m_ftpHost->text().trimmed();
    int port = m_ftpPort->value();
    QString user = m_ftpUser->text().trimmed();
    QString pass = m_ftpPass->text();
    bool useTls = m_ftpTls->isChecked();

    if (host.isEmpty()) {
        QMessageBox::warning(this, "FTP测试", "服务器地址不能为空");
        return;
    }
    if (user.isEmpty()) {
        QMessageBox::warning(this, "FTP测试", "用户名不能为空");
        return;
    }
    if (pass.isEmpty()) {
        Config& cfg = Config::instance();
        cfg.load();
        QJsonObject ftp = cfg.ftpConfig();
        pass = ftp.value("password").toString();
        if (!pass.isEmpty() && cfg.isPasswordSet()) {
            pass = Crypto::decrypt(pass, cfg.encryptionPassword());
        }
    }

    QMessageBox::information(this, "FTP测试", QString("正在连接 %1:%2 ...").arg(host).arg(port));

    QTcpSocket* sock = new QTcpSocket(this);
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    timeoutTimer.connect(&timeoutTimer, &QTimer::timeout, [sock]() { sock->abort(); });
    timeoutTimer.start(15000);

    sock->connectToHost(host, port);

    QEventLoop loop;
    QObject::connect(sock, &QTcpSocket::readyRead, &loop, &QEventLoop::quit);
    QObject::connect(sock, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred), &loop, &QEventLoop::quit);
    QObject::connect(sock, &QTcpSocket::disconnected, &loop, &QEventLoop::quit);
    loop.exec();
    timeoutTimer.stop();

    if (sock->state() != QAbstractSocket::ConnectedState) {
        QMessageBox::critical(this, "FTP测试", QString("连接失败：%1\n\n"
            "请检查：\n"
            "• 服务器地址和端口是否正确\n"
            "• 网络是否通畅\n"
            "• FTP端口是否被防火墙拦截").arg(sock->errorString()));
        sock->deleteLater();
        return;
    }

    QString greeting = QString::fromUtf8(sock->readAll()).trimmed();
    sock->write(QString("USER " + user + "\r\n").toUtf8());

    QEventLoop loop2;
    QObject::connect(sock, &QTcpSocket::readyRead, &loop2, &QEventLoop::quit);
    QTimer::singleShot(5000, &loop2, &QEventLoop::quit);
    loop2.exec();

    QString resp = QString::fromUtf8(sock->readAll()).trimmed();
    QStringList lines = resp.split('\n', Qt::SkipEmptyParts);
    QString userResp;
    for (const QString& line : lines) {
        if (line.trimmed().startsWith("331")) {
            userResp = line.trimmed();
            break;
        }
    }
    if (userResp.isEmpty()) userResp = resp;

    if (!userResp.startsWith("331")) {
        QMessageBox::critical(this, "FTP测试",
            QString("用户名验证失败：\n%1\n\n请检查用户名是否正确").arg(userResp));
        sock->write("QUIT\r\n");
        sock->deleteLater();
        return;
    }

    sock->write(QString("PASS " + pass + "\r\n").toUtf8());

    QEventLoop loop3;
    QObject::connect(sock, &QTcpSocket::readyRead, &loop3, &QEventLoop::quit);
    QTimer::singleShot(5000, &loop3, &QEventLoop::quit);
    loop3.exec();

    QString passResp = QString::fromUtf8(sock->readAll()).trimmed();
    QStringList passLines = passResp.split('\n', Qt::SkipEmptyParts);
    QString passResult;
    for (const QString& line : passLines) {
        if (line.startsWith("230")) {
            passResult = line;
            break;
        }
    }
    if (passResult.isEmpty()) passResult = passResp;
    if (!passResult.startsWith("230")) {
        QMessageBox::critical(this, "FTP测试",
            QString("密码验证失败：\n%1\n\n请检查密码是否正确").arg(passResult));
        sock->write("QUIT\r\n");
        sock->deleteLater();
        return;
    }

    sock->write("QUIT\r\n");
    sock->deleteLater();

    QMessageBox::information(this, "FTP测试",
        QString("连接成功！\n\n服务器：%1\n端口：%2\n用户：%3\nTLS：%4")
        .arg(host).arg(port).arg(user).arg(useTls ? "是" : "否"));
}
