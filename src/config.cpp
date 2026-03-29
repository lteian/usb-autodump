#include "config.h"
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QCoreApplication>
#include <QMutexLocker>

Config& Config::instance() {
    static Config inst;
    return inst;
}

QString Config::configFilePath() const {
    return QDir(QCoreApplication::applicationDirPath()).filePath("config.json");
}

void Config::load() {
    QMutexLocker locker(&m_mutex);
    QFile f(configFilePath());
    if (!f.open(QIODevice::ReadOnly)) {
        m_data = QJsonObject();
        return;
    }
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError) {
        m_data = QJsonObject();
        return;
    }
    m_data = doc.object();
}

void Config::save() {
    QMutexLocker locker(&m_mutex);
    QFile f(configFilePath());
    if (!f.open(QIODevice::WriteOnly)) return;
    f.write(QJsonDocument(m_data).toJson(QJsonDocument::Indented));
}

void Config::setData(const QJsonObject& data) {
    QMutexLocker locker(&m_mutex);
    m_data = data;
    save();
}

QString Config::localPath(const QString& drive) const {
    QMutexLocker locker(&m_mutex);
    QMap<QString, QString> paths;
    QJsonObject up = m_data.value("usb_paths").toObject();
    for (auto it = up.begin(); it != up.end(); ++it) {
        paths[it.key()] = it.value().toString();
    }
    if (!drive.isEmpty() && paths.contains(drive)) {
        return paths[drive];
    }
    return m_data.value("local_path").toString("D:/U盘转储");
}

QJsonObject Config::ftpConfig() const {
    QMutexLocker locker(&m_mutex);
    return m_data.value("ftp").toObject();
}

bool Config::isPasswordSet() const {
    QMutexLocker locker(&m_mutex);
    return !m_data.value("encryption_password").toString().isEmpty();
}

void Config::setEncryptionPassword(const QString& pwd) {
    QMutexLocker locker(&m_mutex);
    m_data["encryption_password"] = pwd;
    save();
}

QString Config::encryptionPassword() const {
    QMutexLocker locker(&m_mutex);
    return m_data.value("encryption_password").toString();
}

QString Config::passwordVerificationToken() const {
    QMutexLocker locker(&m_mutex);
    return m_data.value("password_verification_token").toString();
}

void Config::setPasswordVerificationToken(const QString& token) {
    QMutexLocker locker(&m_mutex);
    m_data["password_verification_token"] = token;
    save();
}

int Config::maxRetry() const {
    QMutexLocker locker(&m_mutex);
    return m_data.value("ftp").toObject().value("max_retry").toInt(3);
}

bool Config::autoDeleteLocal() const {
    QMutexLocker locker(&m_mutex);
    return m_data.value("auto_delete_local").toBool(true);
}

bool Config::autoFormatAfterCopy() const {
    QMutexLocker locker(&m_mutex);
    return m_data.value("auto_format_after_copy").toBool(false);
}

QStringList Config::videoExtensions() const {
    QMutexLocker locker(&m_mutex);
    QStringList exts;
    QJsonArray arr = m_data.value("video_extensions").toArray();
    for (const QJsonValue& v : arr) exts << v.toString();
    if (exts.isEmpty()) {
        exts << ".mp4" << ".avi" << ".mkv" << ".mov" << ".wmv"
             << ".flv" << ".webm" << ".m4v" << ".mpg" << ".mpeg";
    }
    return exts;
}

QMap<QString, QString> Config::usbPaths() const {
    QMutexLocker locker(&m_mutex);
    QMap<QString, QString> paths;
    QJsonObject up = m_data.value("usb_paths").toObject();
    for (auto it = up.begin(); it != up.end(); ++it) {
        paths[it.key()] = it.value().toString();
    }
    return paths;
}

void Config::clearAll() {
    QMutexLocker locker(&m_mutex);
    QFile::remove(configFilePath());
    m_data = QJsonObject();
}