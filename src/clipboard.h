#ifndef CLIPBOARD_H
#define CLIPBOARD_H

#include <QObject>
#include <QString>
#include <QTimer>

// Copies secrets and clears them again after a while, unless something
// else has been copied since
class Clipboard : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString label READ label NOTIFY labelChanged)
    Q_PROPERTY(int remaining READ remaining NOTIFY remainingChanged)

public:
    explicit Clipboard(QObject *parent = nullptr);

    QString label() const { return m_label; }
    int remaining() const { return m_remaining; }

    // clearAfter in seconds, 0 keeps the text. label stays set until the
    // text is cleared or something else is copied.
    Q_INVOKABLE void copy(const QString &text, const QString &label, int clearAfter);
    Q_INVOKABLE void clear();

signals:
    void labelChanged();
    void remainingChanged();

private:
    void tick();
    void reset();

    QString m_text;
    QString m_label;
    int m_remaining;
    QTimer m_timer;
};

#endif // CLIPBOARD_H
