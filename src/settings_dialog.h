#ifndef SETTINGS_DIALOG_H
#define SETTINGS_DIALOG_H

#include <QDialog>
#include <QTabWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QTableWidget>
#include <QLabel>

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

private slots:
    void onBrowseLocalPath();
    void onSave();
    void onResetAll();
    void onTestFTP();

private:
    void loadCurrentConfig();

    QTabWidget* m_tabs = nullptr;

    // Local tab
    QLineEdit* m_localPathEdit = nullptr;
    QTableWidget* m_usbPathsTable = nullptr;

    // FTP tab
    QLineEdit* m_ftpHost = nullptr;
    QSpinBox* m_ftpPort = nullptr;
    QLineEdit* m_ftpUser = nullptr;
    QLineEdit* m_ftpPass = nullptr;
    QLineEdit* m_ftpSubPath = nullptr;
    QCheckBox* m_ftpTls = nullptr;
    QComboBox* m_ftpEncoding = nullptr;

    // Advanced tab
    QCheckBox* m_autoFormat = nullptr;
    QCheckBox* m_autoDelete = nullptr;
    QSpinBox* m_retrySpin = nullptr;

    // Encryption
    QLineEdit* m_encPwdEdit = nullptr;
    QLineEdit* m_encPwd2Edit = nullptr;
    QLabel* m_encError = nullptr;
    QLabel* m_encWarn = nullptr;
};

#endif // SETTINGS_DIALOG_H
