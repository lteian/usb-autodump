#ifndef CONFIG_H
#define CONFIG_H

#include <QString>
#include <QJsonObject>
#include <QMap>
#include <QMutex>

class Config {
public:
    static Config& instance();

    void load();
    void save();
    void setData(const QJsonObject& data);

    QString localPath(const QString& drive = "") const;
    QJsonObject ftpConfig() const;
    bool isPasswordSet() const;
    void setEncryptionPassword(const QString& pwd);
    QString encryptionPassword() const;
    QString passwordVerificationToken() const;
    void setPasswordVerificationToken(const QString& token);

    // convenience
    int maxRetry() const;
    bool autoDeleteLocal() const;
    bool autoFormatAfterCopy() const;
    QStringList videoExtensions() const;
    QMap<QString, QString> usbPaths() const;

    void clearAll();

private:
    Config() = default;
    QString configFilePath() const;

    QJsonObject m_data;
    mutable QMutex m_mutex{QMutex::Recursive};
};

#endif // CONFIG_H
