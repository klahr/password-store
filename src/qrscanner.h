#ifndef QRSCANNER_H
#define QRSCANNER_H

#include <QImage>
#include <QJSValue>
#include <QObject>
#include <QStringList>
#include <QVariant>

// Finds QR codes in camera frames and pictures with quirc, off the main thread
class QrScanner : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    explicit QrScanner(QObject *parent = nullptr);

    bool busy() const { return m_busy; }

    // The text of each QR code in image
    static QStringList decode(const QImage &image);

    // Callback result: texts. A frame arriving while one is decoded is
    // skipped, there is a newer one soon.
    Q_INVOKABLE void scanImage(const QVariant &image, QJSValue callback);
    // Callback result: ok, error, texts
    Q_INVOKABLE void scanFile(const QString &path, QJSValue callback);

signals:
    void busyChanged();

private:
    void scan(const QImage &image, const QJSValue &callback);

    bool m_busy;
};

#endif // QRSCANNER_H
