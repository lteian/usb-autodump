#include <QApplication>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include "mainwindow.h"
#include "config.h"

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    a.setApplicationName("USB自动转储工具");
    a.setOrganizationName("usb-autodump");

    // Ensure config exists
    Config::instance().load();

    // If encryption password is set, verify before showing main window
    if (Config::instance().isPasswordSet()) {
        bool ok = false;
        QString pwd = QInputDialog::getText(
            nullptr,
            "🔐 输入密码",
            "请输入加密密码以访问程序：",
            QLineEdit::Password,
            "", &ok
        );
        if (!ok || pwd.isEmpty()) {
            return 0;
        }
        // Verify password by trying to decrypt a known value
        QString encrypted = Config::instance().ftpConfig().value("password").toString();
        if (!encrypted.isEmpty()) {
            QString decrypted = Crypto::decrypt(encrypted, pwd);
            if (decrypted.isEmpty()) {
                // Password might be wrong but encrypted empty, try stored password directly
                if (pwd != Config::instance().encryptionPassword()) {
                    QMessageBox::warning(nullptr, "错误", "密码错误");
                    return 0;
                }
            }
        } else {
            // No FTP password stored, verify against encryption_password
            if (pwd != Config::instance().encryptionPassword()) {
                QMessageBox::warning(nullptr, "错误", "密码错误");
                return 0;
            }
        }
    }

    MainWindow w;
    w.show();
    return a.exec();
}
