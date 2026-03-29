#include "disk_space_widget.h"
#include "disk_tool.h"
#include <QPainter>
#include <QFont>
#include <QRectF>
#include <QtMath>

DiskSpaceWidget::DiskSpaceWidget(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
}

void DiskSpaceWidget::updateFromPath(const QString& path) {
    m_path = path;
    if (path.length() >= 2 && path[1] == ':') {
        m_driveLetter = QString(path[0]);
    } else {
        m_driveLetter = path.isEmpty() ? "C" : QString(path[0]);
    }

    qint64 freeBytes = 0, totalBytes = 0;
    if (DiskTool::getDiskSpace(path, freeBytes, totalBytes)) {
        m_freeBytes = freeBytes;
        m_totalBytes = totalBytes;
        m_usedBytes = totalBytes - freeBytes;
    } else {
        m_freeBytes = 0;
        m_totalBytes = 0;
        m_usedBytes = 0;
    }
    update();
}

void DiskSpaceWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    int w = width();
    int h = height();

    // Calculate colors based on free percentage
    double freePct = m_totalBytes > 0 ? (double)m_freeBytes / m_totalBytes * 100.0 : 0;
    QColor usedColor;
    if (freePct < 10) {
        usedColor = QColor("#EF4444");   // red
    } else if (freePct < 20) {
        usedColor = QColor("#F59E0B");   // orange
    } else if (freePct < 50) {
        usedColor = QColor("#F59E0B");   // orange
    } else {
        usedColor = QColor("#3B82F6");   // blue
    }
    QColor freeColor = QColor("#10B981"); // green
    QColor bgColor = QColor("#E2E8F0");

    // Donut chart - centered, with percentage in center
    int legendH = 24;
    int availableH = h - legendH;
    int size = qMin(w, availableH);
    int cx = w / 2;
    int cy = size / 2;
    int radius = size / 2 - 4;

    if (m_totalBytes > 0) {
        double usedFraction = (double)m_usedBytes / m_totalBytes;
        double freeFraction = (double)m_freeBytes / m_totalBytes;

        QRectF rect(cx - radius, cy - radius, radius * 2, radius * 2);
        p.setPen(Qt::NoPen);

        // Used slice
        if (usedFraction > 0) {
            p.setBrush(usedColor);
            int spanAngle = static_cast<int>(-usedFraction * 360 * 16);
            int startAngle = 90 * 16;
            p.drawPie(rect, startAngle, spanAngle);
        }

        // Free slice
        if (freeFraction > 0) {
            p.setBrush(freeColor);
            int spanAngle = static_cast<int>(-freeFraction * 360 * 16);
            int startAngle = 90 * 16 - static_cast<int>(-usedFraction * 360 * 16);
            p.drawPie(rect, startAngle, spanAngle);
        }

        // White center circle (donut hole)
        p.setBrush(QColor("#FFFFFF"));
        int innerRadius = radius * 0.62;
        p.drawEllipse(QRectF(cx - innerRadius, cy - innerRadius, innerRadius * 2, innerRadius * 2));

        // Center: drive letter + percentage
        p.setPen(QPen(QColor("#1E293B")));
        QFont f = font();
        f.setPointSizeF(16);
        f.setWeight(QFont::Bold);
        p.setFont(f);
        p.drawText(QRectF(cx - innerRadius, cy - 20, innerRadius * 2, 22), Qt::AlignCenter, m_driveLetter + ":");

        f.setPointSizeF(11);
        f.setWeight(QFont::Normal);
        p.setFont(f);
        p.drawText(QRectF(cx - innerRadius, cy + 2, innerRadius * 2, 18), Qt::AlignCenter, QString::number(100.0 - freePct, 'f', 0) + "%");
    } else {
        // Empty state
        p.setPen(Qt::NoPen);
        p.setBrush(bgColor);
        p.drawEllipse(QRectF(cx - radius, cy - radius, radius * 2, radius * 2));

        p.setBrush(QColor("#FFFFFF"));
        int innerRadius = radius * 0.62;
        p.drawEllipse(QRectF(cx - innerRadius, cy - innerRadius, innerRadius * 2, innerRadius * 2));

        p.setPen(QPen(QColor("#94A3B8")));
        QFont f = font();
        f.setPointSizeF(12);
        f.setWeight(QFont::Normal);
        p.setFont(f);
        p.drawText(QRectF(cx - innerRadius, cy - 10, innerRadius * 2, 20), Qt::AlignCenter, "无数据");
    }

    // Bottom legend
    int labelY = size + 6;
    int labelW = w / 2;

    // Used legend
    p.setPen(Qt::NoPen);
    p.setBrush(usedColor);
    p.drawEllipse(QRectF(labelW / 2 - 28, labelY, 8, 8));
    p.setPen(QPen(QColor("#64748B")));
    QFont f2 = font();
    f2.setPointSizeF(10);
    f2.setWeight(QFont::Normal);
    p.setFont(f2);
    p.drawText(labelW / 2 - 16, labelY + 9, "已用");

    // Free legend
    p.setPen(Qt::NoPen);
    p.setBrush(freeColor);
    p.drawEllipse(QRectF(labelW + labelW / 2 - 28, labelY, 8, 8));
    p.setPen(QColor("#64748B"));
    p.drawText(labelW + labelW / 2 - 16, labelY + 9, "可用");
}

void DiskSpaceWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    update();
}

QString DiskSpaceWidget::formatSize(qint64 b) const {
    if (b >= 1024LL * 1024 * 1024) return QString::number(b / 1024.0 / 1024 / 1024, 'f', 1) + "GB";
    if (b >= 1024 * 1024) return QString::number(b / 1024.0 / 1024, 'f', 0) + "MB";
    if (b >= 1024) return QString::number(b / 1024.0, 'f', 0) + "KB";
    return QString::number(b) + "B";
}