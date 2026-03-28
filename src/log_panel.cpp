#include "log_panel.h"
#include <QDateTime>
#include <QTextCharFormat>
#include <QDebug>

LogPanel::LogPanel(QWidget* parent)
    : QTextEdit(parent)
{
    setReadOnly(true);
    setMaximumHeight(120);
    setStyleSheet(R"(
        QTextEdit {
            background: #FFFFFF;
            color: #1F2329;
            font-family: 'Consolas', 'Courier New', monospace;
            font-size: 12px;
            border: none;
            padding: 4px 2px;
        }
    )");
}

QString LogPanel::ts() const {
    return QDateTime::currentDateTime().toString("HH:mm:ss");
}

void LogPanel::log(const QString& level, const QString& msg, const QString& color) {
    QString html = QString(
        "<span style=\"color: #86909C;\">[%1]</span> "
        "<span style=\"color: %2;\">%3</span> "
        "<span style=\"color: #1F2329;\">%4</span>"
    ).arg(ts()).arg(color).arg(level.toUpper()).arg(msg);
    append(html);
}

void LogPanel::appendInfo(const QString& msg)    { log("INFO", msg, "#165DFF"); }
void LogPanel::appendWarning(const QString& msg) { log("WARN", msg, "#FF7D00"); }
void LogPanel::appendError(const QString& msg)  { log("ERRO", msg, "#F53F3F"); }
void LogPanel::appendDebug(const QString& msg)   { log("DEBU", msg, "#86909C"); }
