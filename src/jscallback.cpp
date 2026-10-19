#include "jscallback.h"

#include <QJSEngine>
#include <QPointer>
#include <QDebug>

namespace {
QPointer<QJSEngine> s_engine;
}

void JsCallback::setEngine(QJSEngine *engine)
{
    s_engine = engine;
}

void JsCallback::call(QJSValue callback, const QVariantMap &result)
{
    if (!callback.isCallable() || !s_engine)
        return;
    QJSValue ret = callback.call(QJSValueList() << s_engine->toScriptValue(result));
    if (ret.isError())
        qWarning() << "Callback failed:" << ret.toString();
}
