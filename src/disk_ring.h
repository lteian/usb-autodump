#ifndef DISK_RING_H
#define DISK_RING_H

#include <QWidget>
#include <QString>
#include <QColor>

class DiskRing : public QWidget {
    Q_OBJECT
public:
    explicit DiskRing(QWidget* parent = nullptr);

    void setUsedFraction(double v);
    void setRingColor(const QColor& c);
    void setBgColor(const QColor& c);
    void setCenterText(const QString& t);
    void setRingWidth(int w);
    void setDiameter(int d);
    void setDriveLetter(const QString& l);
    void setFreeBytes(qint64 b);
    void setTotalBytes(qint64 b);
    void updateFromPath(const QString& path, qint64 free, qint64 total);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    void updateText();
    QString formatSize(qint64 b) const;

    double m_usedFraction = 0.0;
    QColor m_ringColor = QColor("#3B82F6");
    QColor m_bgColor = QColor("#E5E7EB");
    QString m_centerText;
    int m_ringWidth = 4;
    int m_diameter = 44;
    QString m_driveLetter;
    qint64 m_freeBytes = 0;
    qint64 m_totalBytes = 0;
};

#endif // DISK_RING_H
