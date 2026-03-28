#include "disk_ring.h"
#include <QPainter>
#include <QPen>
#include <QFont>

DiskRing::DiskRing(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    m_diameter = 36;
    m_ringWidth = 4;
    setFixedSize(m_diameter, m_diameter);
}

void DiskRing::setDiameter(int d) {
    m_diameter = d;
    setFixedSize(d, d);
    update();
}

void DiskRing::setRingWidth(int w) {
    m_ringWidth = w;
    update();
}

void DiskRing::setUsedFraction(double v) {
    m_usedFraction = v;
    update();
}

void DiskRing::setRingColor(const QColor& c) {
    m_ringColor = c;
    update();
}

void DiskRing::setBgColor(const QColor& c) {
    m_bgColor = c;
    update();
}

void DiskRing::setCenterText(const QString& t) {
    m_centerText = t;
    update();
}

void DiskRing::setDriveLetter(const QString& l) {
    m_driveLetter = l;
    update();
}

void DiskRing::setFreeBytes(qint64 b) {
    m_freeBytes = b;
    updateText();
    update();
}

void DiskRing::setTotalBytes(qint64 b) {
    m_totalBytes = b;
    updateText();
    update();
}

void DiskRing::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int w = m_diameter;
    int penW = m_ringWidth;
    int r = (w - penW) / 2;
    QPointF center(w / 2.0, w / 2.0);

    // Background ring
    QPen bgPen;
    bgPen.setColor(m_bgColor);
    bgPen.setWidth(penW);
    bgPen.setCapStyle(Qt::RoundCap);
    p.setPen(bgPen);
    p.drawEllipse(center, r, r);

    // Used arc
    if (m_usedFraction > 0) {
        QPen ringPen;
        ringPen.setColor(m_ringColor);
        ringPen.setWidth(penW);
        ringPen.setCapStyle(Qt::RoundCap);
        p.setPen(ringPen);
        QRectF rect(center.x() - r, center.y() - r, r * 2, r * 2);
        int spanAngle = static_cast<int>(-m_usedFraction * 360 * 16);
        int startAngle = 90 * 16; // start from top
        p.drawArc(rect, startAngle, spanAngle);
    }

    // Center text
    if (!m_centerText.isEmpty()) {
        p.setPen(QPen(QColor("#1F2329")));
        QFont f = font();
        f.setPointSizeF(7);
        f.setWeight(QFont::Medium);
        p.setFont(f);
        p.drawText(rect(), Qt::AlignCenter, m_centerText);
    }
}

void DiskRing::updateText() {
    if (m_freeBytes <= 0 || m_totalBytes <= 0) {
        m_centerText = "";
        return;
    }
    m_usedFraction = 1.0 - (double)m_freeBytes / (double)m_totalBytes;
    m_usedFraction = qBound(0.0, m_usedFraction, 1.0);

    double freePct = (double)m_freeBytes / (double)m_totalBytes * 100.0;
    if (freePct < 10) {
        m_ringColor = QColor("#F53F3F");   // error red
    } else if (freePct < 20) {
        m_ringColor = QColor("#FF7D00");    // warning orange
    } else {
        m_ringColor = QColor("#165DFF");   // primary blue
    }

    m_centerText = formatSize(m_freeBytes);
}

QString DiskRing::formatSize(qint64 b) const {
    if (b >= 1024*1024*1024) return QString::number(b/1024.0/1024/1024, 'f', 1) + "G";
    if (b >= 1024*1024) return QString::number(b/1024.0/1024, 'f', 0) + "M";
    if (b >= 1024) return QString::number(b/1024.0, 'f', 0) + "K";
    return QString::number(b);
}

void DiskRing::updateFromPath(const QString& path, qint64 free, qint64 total) {
    m_freeBytes = free;
    m_totalBytes = total;
    if (path.length() >= 2 && path[1] == ':') {
        m_driveLetter = path[0];
    }
    updateText();
    update();
}
