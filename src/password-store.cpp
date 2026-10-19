#include "clipboard.h"
#include "git.h"
#include "jscallback.h"
#include "passstore.h"
#include "passwordentry.h"
#include "passwordgenerator.h"
#include "pgp.h"

#include <sailfishapp.h>

#include <QDir>
#include <QGuiApplication>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickView>
#include <QScopedPointer>
#include <QStandardPaths>
#include <QtQml>

int main(int argc, char *argv[])
{
    // git runs this binary as ssh and to ask for the remote password
    const int helperResult = Git::runHelper(argc, argv);
    if (helperResult >= 0)
        return helperResult;

    QScopedPointer<QGuiApplication> app(SailfishApp::application(argc, argv));
    app->setOrganizationName(QStringLiteral("rs.r8"));
    app->setApplicationName(QStringLiteral("password-store"));
    app->setApplicationVersion(QStringLiteral(APP_VERSION));

    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    QFile::setPermissions(dataDir, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);

    qmlRegisterUncreatableType<PassStore>("rs.r8.passwordstore", 1, 0, "PassStore",
                                          QStringLiteral("Use the store context property"));

    Pgp pgp(dataDir + QStringLiteral("/keys"));
    Git git(dataDir);
    PassStore store(dataDir + QStringLiteral("/store"), &pgp, &git);
    PasswordEntry entryParser;
    PasswordGenerator generator;
    Clipboard clipboard;

    QScopedPointer<QQuickView> view(SailfishApp::createView());
    JsCallback::setEngine(view->engine());
    QQmlContext *context = view->rootContext();
    context->setContextProperty(QStringLiteral("pgp"), &pgp);
    context->setContextProperty(QStringLiteral("git"), &git);
    context->setContextProperty(QStringLiteral("store"), &store);
    context->setContextProperty(QStringLiteral("entryParser"), &entryParser);
    context->setContextProperty(QStringLiteral("generator"), &generator);
    context->setContextProperty(QStringLiteral("clipboard"), &clipboard);
    view->setSource(SailfishApp::pathToMainQml());
    view->show();

    const int result = app->exec();
    // Don't leave a copied password behind
    clipboard.clear();
    return result;
}
