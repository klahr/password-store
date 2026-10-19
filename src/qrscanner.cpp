#include "qrscanner.h"
#include "jscallback.h"

#include <QFutureWatcher>
#include <QVariantMap>
#include <QtConcurrent>

#include <quirc.h>

#include <cstring>

QrScanner::QrScanner(QObject *parent)
    : QObject(parent)
    , m_busy(false)
{
}

QStringList QrScanner::decode(const QImage &image)
{
    QStringList texts;
    if (image.isNull())
        return texts;
    const QImage gray = image.convertToFormat(QImage::Format_Grayscale8);

    struct quirc *q = quirc_new();
    if (!q)
        return texts;
    if (quirc_resize(q, gray.width(), gray.height()) < 0) {
        quirc_destroy(q);
        return texts;
    }
    int width = 0;
    int height = 0;
    uint8_t *buffer = quirc_begin(q, &width, &height);
    for (int y = 0; y < height; ++y)
        memcpy(buffer + y * width, gray.constScanLine(y), size_t(width));
    quirc_end(q);

    const int count = quirc_count(q);
    for (int i = 0; i < count; ++i) {
        struct quirc_code code;
        struct quirc_data data;
        quirc_extract(q, i, &code);
        quirc_decode_error_t error = quirc_decode(&code, &data);
        // Codes seen in a mirror decode once flipped
        if (error == QUIRC_ERROR_DATA_ECC) {
            quirc_flip(&code);
            error = quirc_decode(&code, &data);
        }
        if (error == QUIRC_SUCCESS)
            texts << QString::fromUtf8(reinterpret_cast<const char *>(data.payload), data.payload_len);
    }
    quirc_destroy(q);
    return texts;
}

void QrScanner::scan(const QImage &image, const QJSValue &callback)
{
    m_busy = true;
    emit busyChanged();
    QFutureWatcher<QStringList> *watcher = new QFutureWatcher<QStringList>(this);
    connect(watcher, &QFutureWatcher<QStringList>::finished, this, [this, watcher, callback]() {
        const QStringList texts = watcher->result();
        watcher->deleteLater();
        m_busy = false;
        emit busyChanged();
        QVariantMap map;
        map.insert(QStringLiteral("ok"), true);
        map.insert(QStringLiteral("texts"), texts);
        JsCallback::call(callback, map);
    });
    watcher->setFuture(QtConcurrent::run(&QrScanner::decode, image));
}

void QrScanner::scanImage(const QVariant &image, QJSValue callback)
{
    if (m_busy)
        return;
    scan(image.value<QImage>(), callback);
}

void QrScanner::scanFile(const QString &path, QJSValue callback)
{
    QImage image(path);
    if (image.isNull()) {
        QVariantMap map;
        map.insert(QStringLiteral("ok"), false);
        map.insert(QStringLiteral("error"), tr("Could not read %1").arg(path));
        JsCallback::call(callback, map);
        return;
    }
    scan(image, callback);
}
