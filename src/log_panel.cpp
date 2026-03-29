#include "log_panel.h"
#include <QDateTime>
#include <QTextCharFormat>
#include <QDebug>

LogPanel::LogPanel(QWidget* parent)
    : QTextEdit(parent)
{
    setReadOnly(true);
    setStyleSheet(R"(
        QTextEdit {
            background: #F8FAFC;
            color: #475569;
            font-family: 'Cascadia Code', 'Consolas', 'Courier New', monospace;
            font-size: 11px;
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
        "<span style=\"color: #94A3B8;\">[%1]</span> "
        "<span style=\"color: %2;\">%3</span> "
        "<span style=\"color: #475569;\">%4</span>"
    ).arg(ts()).arg(color).arg(level.toUpper()).arg(msg);
    append(html);
}

void LogPanel::appendInfo(const QString& msg)    { log("INFO", msg, "#3B82F6"); }
void LogPanel::appendWarning(const QString& msg) { log("WARN", msg, "#F59E0B"); }
void LogPanel::appendError(const QString& msg)  { log("ERRO", msg, "#EF4444"); }
void LogPanel::appendDebug(const QString& msg)   { log("DEBU", msg, "#94A3B8"); }
