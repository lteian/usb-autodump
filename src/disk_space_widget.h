#ifndef DISK_SPACE_WIDGET_H
#define DISK_SPACE_WIDGET_H

#include <QWidget>
#include <QString>
#include <QColor>

class DiskSpaceWidget : public QWidget {
    Q_OBJECT
public:
    explicit DiskSpaceWidget(QWidget* parent = nullptr);

    void updateFromPath(const QString& path);

private:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent* event) override;
    QString formatSize(qint64 b) const;

    qint64 m_freeBytes = 0;
    qint64 m_totalBytes = 0;
    qint64 m_usedBytes = 0;
    QString m_driveLetter;
    QString m_path;
};

#endif // DISK_SPACE_WIDGET_H