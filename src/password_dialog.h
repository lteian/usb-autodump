#ifndef PASSWORD_DIALOG_H
#define PASSWORD_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>

class PasswordDialog : public QDialog {
    Q_OBJECT
public:
    enum Mode { Verify, SetNew };
    explicit PasswordDialog(Mode mode = Verify, QWidget* parent = nullptr);

    bool wasReset() const { return m_wasReset; }

private slots:
    void onOk();
    void onForgotPassword();

private:
    Mode m_mode;
    QLineEdit* m_pwdEdit = nullptr;
    QPushButton* m_forgotBtn = nullptr;
    QLabel* m_errorLabel = nullptr;
    bool m_wasReset = false;
};

#endif // PASSWORD_DIALOG_H
