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
            emit deviceInserted(currentMap[letter]);
        }
    }

    // Detect removed
    for (const QString& letter : m_lastDevices.keys()) {
        if (!currentMap.contains(letter)) {
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

    emit debugMessage("WMIC raw output: " + output);

    // Some WMIC versions output header and data on same line, or without proper newlines
    // Try to find drive letter patterns anywhere in output
    QRegExp rxDrive("([A-Za-z]:)\\s+(\\d+)\\s+(\\d+)(?:\\s+(\\S+))?");
    int pos = 0;
    while ((pos = rxDrive.indexIn(output, pos)) >= 0) {
        QStringList caps = rxDrive.capturedTexts();
        if (caps.size() >= 4) {
            USBDevice dev;
            dev.driveLetter = caps[1];
            dev.freeSpace = caps[2].toLongLong();
            dev.totalSize = caps[3].toLongLong();
            dev.label = caps.size() > 4 && !caps[4].isEmpty() ? caps[4] : "U盘";
            emit debugMessage(QString("检测到USB设备: %1 标签: %2 大小: %3").arg(dev.driveLetter).arg(dev.label).arg(dev.totalSize));
            if (dev.totalSize > 0) list << dev;
        }
        pos += rxDrive.matchedLength();
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
