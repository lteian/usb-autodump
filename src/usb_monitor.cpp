#include "usb_monitor.h"
#include <QProcess>
#include <QDebug>
#include <QRegularExpression>

#ifdef _WIN32
#include <windows.h>
#endif

USBMonitor::USBMonitor(QObject* parent)
    : QObject(parent)
{
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &USBMonitor::poll);
    m_timer->start(1000);

    // Initial detection
    m_lastDevices = QMap<QString, USBDevice>();
    QList<USBDevice> initial = detectDevices();
    for (const USBDevice& dev : initial) {
        m_lastDevices[dev.driveLetter] = dev;
    }
}

USBMonitor::~USBMonitor() {
    m_timer->stop();
}

QList<USBDevice> USBMonitor::currentDevices() {
    return detectDevices();
}

void USBMonitor::poll() {
    QList<USBDevice> current = detectDevices();
    QMap<QString, USBDevice> currentMap;
    for (const USBDevice& dev : current) {
        currentMap[dev.driveLetter] = dev;
    }

    // Detect inserted
    for (const QString& letter : currentMap.keys()) {
        if (!m_lastDevices.contains(letter)) {
            qDebug() << "USB inserted:" << letter;
            emit deviceInserted(currentMap[letter]);
        }
    }

    // Detect removed
    for (const QString& letter : m_lastDevices.keys()) {
        if (!currentMap.contains(letter)) {
            qDebug() << "USB removed:" << letter;
            emit deviceRemoved(letter);
        }
    }

    m_lastDevices = currentMap;
}

QList<USBDevice> USBMonitor::detectDevices() {
    QList<USBDevice> list;

#ifdef _WIN32
    // Use wmic on Windows — output is fixed-width columns, NOT space-delimited
    QProcess p;
    p.start("wmic", QStringList() << "logicaldisk" << "where" << "DriveType=2" << "get" << "DeviceID,VolumeName,Size,FreeSpace");
    p.waitForFinished(5000);
    QString output = QString::fromLocal8Bit(p.readAll());

    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        if (line.trimmed().isEmpty()) continue;
        // Skip header line
        if (line.trimmed().startsWith("DeviceID")) continue;

        // wmic output is fixed-width: DeviceID(12) VolumeName(20) Size(20) FreeSpace(20)
        // Columns are 0-indexed and variable-width on some systems, so use regex extraction
        QRegExp rx("^\\s*([A-Za-z]:)\\s+([^\\s].*?)\\s+(\\d+)\\s+(\\d+)\\s*$");
        if (rx.indexIn(line) < 0) {
            // Fallback: try simpler pattern for lines without VolumeName
            QRegExp rx2("^\\s*([A-Za-z]:)\\s+(\\d+)\\s+(\\d+)\\s*$");
            if (rx2.indexIn(line) >= 0) {
                QStringList caps = rx2.capturedTexts();
                USBDevice dev;
                dev.driveLetter = caps[1];
                dev.totalSize = caps[2].toLongLong();
                dev.freeSpace = caps[3].toLongLong();
                dev.label = "U盘";
                if (dev.totalSize > 0) list << dev;
            }
            continue;
        }

        QStringList caps = rx.capturedTexts();
        USBDevice dev;
        dev.driveLetter = caps[1].trimmed();
        dev.label = caps[2].trimmed();
        if (dev.label.isEmpty()) dev.label = "U盘";
        dev.totalSize = caps[3].toLongLong();
        dev.freeSpace = caps[4].toLongLong();
        if (dev.totalSize > 0) list << dev;
    }
#else
    // Linux: use lsblk
    QProcess p;
    p.start("lsblk", QStringList() << "-o" << "NAME,MOUNTPOINT,SIZE,TYPE" << "-J" << "-l");
    p.waitForFinished(5000);
    QString output = QString::fromLocal8Bit(p.readAll());

    // Simple parsing - look for /dev/sd* that are mounted
    QProcess p2;
    p2.start("df", QStringList() << "-T");
    p2.waitForFinished(3000);
    QString dfOutput = QString::fromLocal8Bit(p2.readAll());
    QStringList dfLines = dfOutput.split('\n', Qt::SkipEmptyParts);

    for (const QString& l : dfLines) {
        if (l.startsWith("/dev/sd")) {
            QStringList cols = l.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (cols.size() >= 3) {
                QString path = cols[0];
                QString mount = cols[1];
                QString total = cols[2];
                if (mount.startsWith("/media") || mount.startsWith("/mnt") || mount.startsWith("/run/media")) {
                    USBDevice dev;
                    dev.driveLetter = path; // e.g. /dev/sdb1
                    dev.totalSize = total.toLongLong() * 1024; // df gives KB
                    list << dev;
                }
            }
        }
    }
#endif

    return list;
}
