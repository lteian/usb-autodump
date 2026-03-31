#ifndef USB_CARD_H
#define USB_CARD_H

#include <QFrame>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>

class USBCard : public QFrame {
    Q_OBJECT
public:
    explicit USBCard(QWidget* parent = nullptr);
    ~USBCard();

    void setDrive(const QString& letter, const QString& label,
                  qint64 total, qint64 used, qint64 freeSpace);
    void setStatus(const QString& s); // idle/copying/done/formatting

    void updateProgress(int done, int total,
                        double fileProgress,
                        double speedMBps,
                        int etaSeconds,
                        const QString& currentFile);

    void clear();

    QString drive() const { return m_drive; }

signals:
    void formatClicked(const QString& drive);
    void ejectClicked(const QString& drive);
    void cancelDumpClicked(const QString& drive);

private:
    QString m_drive;
    QFrame* m_statusBar = nullptr;
    QLabel* m_driveLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_hintLabel = nullptr;
    QLabel* m_sizeLabel = nullptr;
    QProgressBar* m_capacityBar = nullptr;
    QProgressBar* m_overallProgress = nullptr;
    QLabel* m_speedLabel = nullptr;
    QLabel* m_etaLabel = nullptr;
    QLabel* m_currentFileLabel = nullptr;
    QPushButton* m_formatBtn = nullptr;
    QPushButton* m_ejectBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;

    static QString fmtSize(qint64 b);
};

#endif // USB_CARD_H
