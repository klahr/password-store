#ifndef JSCALLBACK_H
#define JSCALLBACK_H

#include <QJSValue>
#include <QVariantMap>

class QJSEngine;

// Calls back into QML with a result map, the engine is set once from main()
namespace JsCallback {
void setEngine(QJSEngine *engine);
void call(QJSValue callback, const QVariantMap &result);
}

#endif // JSCALLBACK_H
