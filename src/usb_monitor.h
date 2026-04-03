#ifndef USB_MONITOR_H
#define USB_MONITOR_H

#include <QObject>
#include <QMap>
#include <QString>
#include <QTimer>
#include <QList>

struct USBDevice {
    QString driveLetter;
    QString label;
    qint64 totalSize = 0;
    qint64 freeSpace = 0;
    qint64 usedSize() const { return totalSize - freeSpace; }
    double usedPercent() const { return totalSize ? usedSize() * 100.0 / totalSize : 0; }
};

class USBMonitor : public QObject {
    Q_OBJECT
public:
    explicit USBMonitor(QObject* parent = nullptr);
    ~USBMonitor();

    QList<USBDevice> currentDevices();

signals:
    void deviceInserted(const USBDevice& dev);
    void deviceRemoved(const QString& drive);
    void debugMessage(const QString& msg);

private slots:
    void poll();

private:
    QList<USBDevice> detectDevices();

    QTimer* m_timer = nullptr;
    QMap<QString, USBDevice> m_lastDevices;
};

#endif // USB_MONITOR_H
