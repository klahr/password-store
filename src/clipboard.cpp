#include "clipboard.h"

#include <QClipboard>
#include <QGuiApplication>

Clipboard::Clipboard(QObject *parent)
    : QObject(parent)
    , m_remaining(0)
{
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, &Clipboard::tick);
}

void Clipboard::copy(const QString &text, const QString &label, int clearAfter)
{
    QGuiApplication::clipboard()->setText(text);
    m_text = text;
    m_label = label;
    m_remaining = clearAfter;
    emit labelChanged();
    emit remainingChanged();
    if (clearAfter > 0)
        m_timer.start();
    else
        m_timer.stop();
}

void Clipboard::clear()
{
    QClipboard *clipboard = QGuiApplication::clipboard();
    if (!m_text.isEmpty() && clipboard->text() == m_text)
        clipboard->clear();
    reset();
}

void Clipboard::tick()
{
    if (QGuiApplication::clipboard()->text() != m_text) {
        reset();
        return;
    }
    --m_remaining;
    emit remainingChanged();
    if (m_remaining <= 0)
        clear();
}

void Clipboard::reset()
{
    m_timer.stop();
    m_text.fill(QLatin1Char('\0'));
    m_text.clear();
    m_label.clear();
    m_remaining = 0;
    emit labelChanged();
    emit remainingChanged();
}
