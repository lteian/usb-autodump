#include "password_dialog.h"
#include "config.h"
#include "crypto.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QApplication>

PasswordDialog::PasswordDialog(Mode mode, QWidget* parent)
    : QDialog(parent)
    , m_mode(mode)
{
    setWindowTitle("🔐 请输入密码");
    setModal(true);
    setMinimumWidth(400);
    setStyleSheet(R"(
        QDialog { background: #1e1e1e; color: #e0e0e0; }
        QLabel { color: #e0e0e0; }
        QLineEdit { background: #2d2d2d; color: #e0e0e0; border: 1px solid #4a4a4a; padding: 8px; border-radius: 4px; font-size: 14px; }
        QPushButton { background: #424242; color: white; border: none; border-radius: 4px; padding: 8px 20px; }
        QPushButton:hover { background: #616161; }
    )");

    QVBoxLayout* vl = new QVBoxLayout(this);
    vl->setSpacing(16);

    // Icon and title
    QLabel* iconLabel = new QLabel("🔐");
    iconLabel->setStyleSheet("font-size: 48px;");
    iconLabel->setAlignment(Qt::AlignCenter);
    vl->addWidget(iconLabel);

    QLabel* titleLabel = new QLabel(m_mode == SetNew ? "设置新密码" : "请输入密码以访问设置");
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #ffffff;");
    vl->addWidget(titleLabel);

    // Password input
    QFormLayout* fl = new QFormLayout();
    m_pwdEdit = new QLineEdit();
    m_pwdEdit->setEchoMode(QLineEdit::Password);
    m_pwdEdit->setPlaceholderText("请输入密码");
    fl->addRow("密码:", m_pwdEdit);
    vl->addLayout(fl);

    // Error label
    m_errorLabel = new QLabel();
    m_errorLabel->setStyleSheet("color: #F44336; font-size: 13px;");
    m_errorLabel->setAlignment(Qt::AlignCenter);
    m_errorLabel->setVisible(false);
    vl->addWidget(m_errorLabel);

    // Buttons
    QHBoxLayout* btnRow = new QHBoxLayout();
    btnRow->addStretch();

    QPushButton* okBtn = new QPushButton("确定");
    okBtn->setDefault(true);
    okBtn->setStyleSheet("QPushButton { background: #FF9800; color: #1e1e1e; font-weight: bold; } QPushButton:hover { background: #fb8c00; }");
    connect(okBtn, &QPushButton::clicked, this, &PasswordDialog::onOk);
    btnRow->addWidget(okBtn);
    vl->addLayout(btnRow);

    // Forgot password button (only in Verify mode)
    if (m_mode == Verify) {
        m_forgotBtn = new QPushButton("忘记密码？");
        m_forgotBtn->setStyleSheet("QPushButton { background: none; color: #9e9e9e; border: none; font-size: 12px; } QPushButton:hover { color: #b0b0b0; }");
        connect(m_forgotBtn, &QPushButton::clicked, this, &PasswordDialog::onForgotPassword);
        vl->addWidget(m_forgotBtn, 0, Qt::AlignRight);
    }

    vl->addStretch();

    resize(minimumWidth(), 280);
}

void PasswordDialog::onOk() {
    QString pwd = m_pwdEdit->text();
    Config& cfg = Config::instance();
    cfg.load();

    if (m_mode == SetNew) {
        // Validate new password
        if (pwd.length() < 4) {
            m_errorLabel->setText("密码至少4个字符");
            m_errorLabel->setVisible(true);
            return;
        }
        // Set password - encrypt verification token
        QString token = Crypto::encrypt("USB_AUTO_DUMP_VERIFY_TOKEN_v1", pwd);
        cfg.setPasswordVerificationToken(token);
        cfg.setEncryptionPassword(pwd);
        accept();
        return;
    }

    // Verify mode
    QString storedToken = cfg.passwordVerificationToken();
    if (storedToken.isEmpty()) {
        // No password set yet - accept any non-empty password of 4+ chars
        if (pwd.length() >= 4) {
            accept();
        } else {
            m_errorLabel->setText("密码错误或长度不足");
            m_errorLabel->setVisible(true);
        }
        return;
    }

    if (Crypto::verifyPassword(pwd, storedToken)) {
        accept();
    } else {
        m_errorLabel->setText("密码错误");
        m_errorLabel->setVisible(true);
        m_pwdEdit->selectAll();
    }
}

void PasswordDialog::onForgotPassword() {
    QMessageBox warnBox(this);
    warnBox.setWindowTitle("⚠️ 严重警告");
    warnBox.setText("此操作将清空所有配置！\n\n此操作不可恢复！");
    warnBox.setInformativeText(
        "此操作将删除：\n"
        "• 加密密码\n"
        "• FTP 配置\n"
        "• 所有转储记录\n\n"
        "所有数据将被永久清除！");
    warnBox.setStandardButtons(QMessageBox::Cancel | QMessageBox::Ok);
    warnBox.setDefaultButton(QMessageBox::Cancel);
    warnBox.setStyleSheet(R"(
        QMessageBox { background: #1e1e1e; color: #e0e0e0; }
        QLabel { color: #e0e0e0; background: #1e1e1e; }
        QPushButton { background: #424242; color: white; border: none; border-radius: 4px; padding: 6px 16px; }
        QPushButton:hover { background: #616161; }
    )");

    QPushButton* okBtn = warnBox.button(QMessageBox::Ok);
    okBtn->setText("确认清空");
    okBtn->setStyleSheet("QPushButton { background: #b71c1c; color: white; border: none; border-radius: 4px; padding: 6px 16px; font-weight: bold; }");

    int r = warnBox.exec();
    if (r == QMessageBox::Ok) {
        Config::instance().clearAll();
        m_wasReset = true;
        reject();
    }
}
