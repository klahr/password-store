#include "git.h"
#include "jscallback.h"
#include "passstore.h"
#include "passwordentry.h"
#include "passwordgenerator.h"
#include "qrscanner.h"
#include "pgp.h"

#include <QEventLoop>
#include <QJSEngine>
#include <QPainter>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>

class CoreTest : public QObject
{
    Q_OBJECT

private:
    QVariantMap wait(const std::function<void(const Pgp::Callback &)> &start)
    {
        QVariantMap result;
        QEventLoop loop;
        start([&](const QVariantMap &r) {
            result = r;
            loop.quit();
        });
        loop.exec();
        return result;
    }

    QByteArray gpg(const QStringList &args, const QByteArray &input = QByteArray())
    {
        QProcess process;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("GNUPGHOME"), m_gnupgHome.path());
        process.setProcessEnvironment(env);
        process.start(QStringLiteral("gpg"), QStringList() << QStringLiteral("--batch")
                                                           << QStringLiteral("--pinentry-mode")
                                                           << QStringLiteral("loopback") << args);
        process.write(input);
        process.closeWriteChannel();
        process.waitForFinished(60000);
        if (process.exitCode() != 0)
            qWarning() << process.readAllStandardError();
        return process.readAllStandardOutput();
    }

    // Calls a QJSValue-taking method and waits for its callback
    QVariantMap call(QJSEngine &engine, const std::function<void(QJSValue)> &start)
    {
        engine.globalObject().setProperty(QStringLiteral("done"), false);
        start(engine.evaluate(QStringLiteral("(function(r) { result = r; done = true; })")));
        if (!QTest::qWaitFor([&]() { return engine.globalObject().property(QStringLiteral("done")).toBool(); }, 30000))
            return QVariantMap();
        return engine.globalObject().property(QStringLiteral("result")).toVariant().toMap();
    }

    QString run(const QString &dir, const QStringList &args)
    {
        QProcess process;
        process.setWorkingDirectory(dir);
        process.start(QStringLiteral("git"), args);
        process.waitForFinished(30000);
        return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    }

    QTemporaryDir m_gnupgHome;
    QTemporaryDir m_data;

private slots:
    void parseFirstLinePassword()
    {
        const QVariantMap e = PasswordEntry::parse(QStringLiteral(
            "hunter2\nlogin: alice\nurl: https://example.com\nhttps://example.com/login\nsome note\n"));
        QCOMPARE(e.value("password").toString(), QStringLiteral("hunter2"));
        QCOMPARE(e.value("username").toString(), QStringLiteral("alice"));
        const QVariantList fields = e.value("fields").toList();
        QCOMPARE(fields.size(), 1);
        QCOMPARE(fields.first().toMap().value("key").toString(), QStringLiteral("url"));
        QCOMPARE(fields.first().toMap().value("value").toString(), QStringLiteral("https://example.com"));
        QCOMPARE(e.value("extraContent").toString(), QStringLiteral("https://example.com/login\nsome note"));
        QVERIFY(!e.value("hasTotp").toBool());
    }

    void parsePasswordField()
    {
        // Like pass, a later "secret:" line doesn't replace the first line
        QVariantMap e = PasswordEntry::parse(QStringLiteral("hunter2\nuser: bob\nsecret: blue\n"));
        QCOMPARE(e.value("password").toString(), QStringLiteral("hunter2"));
        QCOMPARE(e.value("username").toString(), QStringLiteral("bob"));
        QCOMPARE(e.value("fields").toList().size(), 1);
        // After a TOTP first line, a "password:" line is the password
        e = PasswordEntry::parse(QStringLiteral("otpauth://totp/x?secret=JBSWY3DPEHPK3PXP\nPassword:   s3cret\n"));
        QCOMPARE(e.value("password").toString(), QStringLiteral("s3cret"));
        QVERIFY(e.value("hasTotp").toBool());
    }

    void parseTotpOnly()
    {
        const QVariantMap e = PasswordEntry::parse(QStringLiteral(
            "otpauth://totp/ACME:alice?secret=GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ&issuer=ACME&digits=8&algorithm=SHA256\n"));
        QVERIFY(!e.value("hasPassword").toBool());
        QVERIFY(e.value("hasTotp").toBool());
        const QVariantMap totp = e.value("totp").toMap();
        QCOMPARE(totp.value("digits").toInt(), 8);
        QCOMPARE(totp.value("algorithm").toString(), QStringLiteral("SHA256"));
        QCOMPARE(totp.value("issuer").toString(), QStringLiteral("ACME"));
    }

    // RFC 6238 appendix B
    void totpVectors_data()
    {
        QTest::addColumn<QString>("secret");
        QTest::addColumn<QString>("algorithm");
        QTest::addColumn<qint64>("time");
        QTest::addColumn<QString>("code");
        const QString sha1 = QStringLiteral("GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ");
        const QString sha256 = QStringLiteral("GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQGEZA");
        const QString sha512 = QStringLiteral(
            "GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQGEZDGNA");
        QTest::newRow("sha1 59") << sha1 << "SHA1" << qint64(59) << "94287082";
        QTest::newRow("sha1 1111111109") << sha1 << "SHA1" << qint64(1111111109) << "07081804";
        QTest::newRow("sha256 1234567890") << sha256 << "SHA256" << qint64(1234567890) << "91819424";
        QTest::newRow("sha512 20000000000") << sha512 << "SHA512" << qint64(20000000000) << "47863826";
    }

    void totpVectors()
    {
        QFETCH(QString, secret);
        QFETCH(QString, algorithm);
        QFETCH(qint64, time);
        QFETCH(QString, code);
        QVariantMap totp;
        totp.insert("secret", secret);
        totp.insert("algorithm", algorithm);
        totp.insert("digits", 8);
        totp.insert("period", 30);
        QCOMPARE(PasswordEntry::totpCode(totp, time), code);
    }

    void totpShortSecret()
    {
        const QVariantMap e = PasswordEntry::parse(QStringLiteral("pw\ntotp: JBSW Y3DP EHPK 3PXP\n"));
        QVERIFY(e.value("hasTotp").toBool());
        QCOMPARE(PasswordEntry::totpCode(e.value("totp").toMap(), 0).length(), 6);
    }

    void editRoundTrip()
    {
        const QString content = QStringLiteral("totp: JBSWY3DPEHPK3PXP\nuser: bob\npassword: old\nnote\n");
        const QVariantMap split = PasswordEntry::splitForEdit(content);
        QCOMPARE(split.value("password").toString(), QStringLiteral("old"));
        QCOMPARE(PasswordEntry::joinForEdit(QStringLiteral("new"), split.value("rest").toString(),
                                            split.value("line").toInt(), split.value("prefix").toString()),
                 QStringLiteral("totp: JBSWY3DPEHPK3PXP\nuser: bob\npassword: new\nnote\n"));
        const QVariantMap first = PasswordEntry::splitForEdit(QStringLiteral("old\nsecret: blue\n"));
        QCOMPARE(PasswordEntry::joinForEdit(QStringLiteral("new"), first.value("rest").toString(),
                                            first.value("line").toInt(), first.value("prefix").toString()),
                 QStringLiteral("new\nsecret: blue\n"));
        const QVariantMap simple = PasswordEntry::splitForEdit(QStringLiteral("pw\nextra\n"));
        QCOMPARE(PasswordEntry::joinForEdit(QStringLiteral("pw2"), simple.value("rest").toString(),
                                            simple.value("line").toInt(), simple.value("prefix").toString()),
                 QStringLiteral("pw2\nextra\n"));
    }

    void generator()
    {
        PasswordGenerator generator;
        for (int i = 0; i < 50; ++i) {
            const QString pw = generator.classic(20, true, true, true, true, true);
            QCOMPARE(pw.length(), 20);
            QVERIFY(pw.contains(QRegularExpression("[0-9]")));
            QVERIFY(pw.contains(QRegularExpression("[A-Z]")));
            QVERIFY(pw.contains(QRegularExpression("[a-z]")));
            QVERIFY(pw.contains(QRegularExpression("[^0-9A-Za-z]")));
            QVERIFY(!pw.contains(QRegularExpression("[B8G6I1l0OQDS5Z2]")));
        }
        QVERIFY(generator.classic(2, true, true, true, false, false).isEmpty());
        QVERIFY(generator.classic(20, false, false, false, false, false).isEmpty());
        QCOMPARE(generator.classic(20, true, false, false, false, false).length(), 20);
    }

    void gpgInterop()
    {
        if (QStandardPaths::findExecutable(QStringLiteral("gpg")).isEmpty())
            QSKIP("gpg not installed");
        gpg(QStringList() << "--passphrase" << "test" << "--quick-gen-key" << "Test User <test@example.com>"
                          << "default" << "default" << "never");
        const QByteArray secretKey = gpg(QStringList() << "--passphrase" << "test" << "--armor"
                                                       << "--export-secret-keys" << "test@example.com");
        QVERIFY(secretKey.contains("PGP PRIVATE KEY BLOCK"));

        const QString encrypted = m_data.filePath("gpg.gpg");
        gpg(QStringList() << "--trust-model" << "always" << "-r" << "test@example.com" << "-o" << encrypted
                          << "--encrypt",
            "correct horse\nuser: me\n");
        QVERIFY(QFile::exists(encrypted));

        Pgp pgp(m_data.filePath("keys"));
        QVariantMap r = wait([&](const Pgp::Callback &cb) { pgp.decrypt(encrypted, cb); });
        QVERIFY(!r.value("ok").toBool());
        QVERIFY(!r.value("needPassphrase").toBool());

        QFile keyFile(m_data.filePath("secret.asc"));
        QVERIFY(keyFile.open(QIODevice::WriteOnly));
        keyFile.write(secretKey);
        keyFile.close();
        // A key file encrypted with gpg --symmetric needs its passphrase first
        const QString encryptedKey = m_data.filePath("secret.key.gpg");
        gpg(QStringList() << "--passphrase" << "filepass" << "--symmetric" << "-o" << encryptedKey << keyFile.fileName());
        QVERIFY(QFile::exists(encryptedKey));
        QJSEngine engine;
        JsCallback::setEngine(&engine);
        r = call(engine, [&](QJSValue cb) { pgp.importKeys(encryptedKey, "", cb); });
        QVERIFY(r.value("needPassphrase").toBool());
        r = call(engine, [&](QJSValue cb) { pgp.importKeys(encryptedKey, "nope", cb); });
        QVERIFY(r.value("needPassphrase").toBool());
        QCOMPARE(r.value("error").toString(), QStringLiteral("Wrong passphrase"));
        QCOMPARE(pgp.secretKeyCount(), 0);
        r = call(engine, [&](QJSValue cb) { pgp.importKeys(encryptedKey, "filepass", cb); });
        QVERIFY2(r.value("ok").toBool(), qPrintable(r.value("error").toString()));
        QCOMPARE(r.value("imported").toInt(), 1);
        r = call(engine, [&](QJSValue cb) { pgp.importKeys(QStringLiteral("not a key"), "", cb); });
        QCOMPARE(r.value("error").toString(), QStringLiteral("No OpenPGP keys found"));
        r = wait([&](const Pgp::Callback &cb) { pgp.decrypt(encrypted, cb); });
        QVERIFY(!r.value("ok").toBool());
        QVERIFY(r.value("needPassphrase").toBool());
        QCOMPARE(r.value("keyName").toString(), QStringLiteral("Test User <test@example.com>"));
        QCOMPARE(pgp.secretKeyCount(), 1);

        pgp.setPassphrase(QStringLiteral("wrong"));
        r = wait([&](const Pgp::Callback &cb) { pgp.decrypt(encrypted, cb); });
        QVERIFY(r.value("needPassphrase").toBool());
        QVERIFY(!pgp.hasPassphrase());

        pgp.setPassphrase(QStringLiteral("test"));
        r = wait([&](const Pgp::Callback &cb) { pgp.decrypt(encrypted, cb); });
        QVERIFY2(r.value("ok").toBool(), qPrintable(r.value("error").toString()));
        QCOMPARE(r.value("text").toString(), QStringLiteral("correct horse\nuser: me\n"));

        // And back: rnp encrypts, gpg decrypts. .gpg-id may hold an email.
        const QString ours = m_data.filePath("sub/ours.gpg");
        r = wait([&](const Pgp::Callback &cb) {
            pgp.encrypt(ours, QStringList() << "test@example.com", QStringLiteral("from rnp\n"), cb);
        });
        QVERIFY2(r.value("ok").toBool(), qPrintable(r.value("error").toString()));
        QCOMPARE(gpg(QStringList() << "--passphrase" << "test" << "--decrypt" << ours), QByteArray("from rnp\n"));

        r = wait([&](const Pgp::Callback &cb) {
            pgp.encrypt(ours, QStringList() << "nobody@example.com", QStringLiteral("x"), cb);
        });
        QVERIFY(!r.value("ok").toBool());
        QCOMPARE(r.value("missing").toStringList(), QStringList() << "nobody@example.com");

        // The keyring survives a restart
        Pgp reloaded(m_data.filePath("keys"));
        reloaded.setPassphrase(QStringLiteral("test"));
        r = wait([&](const Pgp::Callback &cb) { reloaded.decrypt(encrypted, cb); });
        QVERIFY2(r.value("ok").toBool(), qPrintable(r.value("error").toString()));
    }

    void storeAndGit()
    {
        if (QStandardPaths::findExecutable(QStringLiteral("git")).isEmpty())
            QSKIP("git not installed");
        QTemporaryDir data;
        QJSEngine engine;
        JsCallback::setEngine(&engine);
        Pgp pgp(data.filePath("keys"));
        Git git(data.path());
        git.setAuthMode(QStringLiteral("none"));
        git.setAuthorName(QStringLiteral("Test"));
        git.setAuthorEmail(QStringLiteral("test@example.com"));
        PassStore store(data.filePath("store"), &pgp, &git);

        QVariantMap r = call(engine, [&](QJSValue cb) { pgp.generateKey("Phone <phone@example.com>", "", cb); });
        QVERIFY2(r.value("ok").toBool(), qPrintable(r.value("error").toString()));
        const QString fpr = r.value("fingerprint").toString();

        QVERIFY(!store.initialized());
        r = call(engine, [&](QJSValue cb) { store.init("", QStringList() << fpr, cb); });
        QVERIFY(r.value("ok").toBool());
        QVERIFY(store.initialized());
        r = call(engine, [&](QJSValue cb) { git.init(cb); });
        QVERIFY2(r.value("ok").toBool(), qPrintable(r.value("error").toString()));
        QVERIFY(git.isRepo());

        r = call(engine, [&](QJSValue cb) { store.save("web/example.com", "pw1\nuser: me\n", "", cb); });
        QVERIFY2(r.value("ok").toBool() && !r.contains("gitError"), qPrintable(r.value("gitError").toString()));
        r = call(engine, [&](QJSValue cb) { store.save("web/example.com", "x\n", "", cb); });
        QVERIFY(!r.value("ok").toBool());
        r = call(engine, [&](QJSValue cb) { store.save("bank", "pw2\n", "", cb); });
        QVERIFY(r.value("ok").toBool());

        const QVariantList root = store.list("", PassStore::FolderFirst, false);
        QCOMPARE(root.size(), 2);
        QCOMPARE(root.at(0).toMap().value("path").toString(), QStringLiteral("web"));
        QCOMPARE(root.at(1).toMap().value("path").toString(), QStringLiteral("bank"));
        const QVariantList found = store.search("", "exm", PassStore::FolderFirst, false, true);
        QCOMPARE(found.size(), 1);
        QCOMPARE(found.first().toMap().value("path").toString(), QStringLiteral("web/example.com"));
        QCOMPARE(store.search("", "exm", PassStore::FolderFirst, false, false).size(), 0);

        r = call(engine, [&](QJSValue cb) { store.decrypt("web/example.com", cb); });
        QCOMPARE(r.value("text").toString(), QStringLiteral("pw1\nuser: me\n"));

        r = call(engine, [&](QJSValue cb) { store.save("web/example.org", "pw3\n", "web/example.com", cb); });
        QVERIFY(r.value("ok").toBool() && !r.contains("gitError"));
        QVERIFY(!store.exists("web/example.com"));
        r = call(engine, [&](QJSValue cb) { store.move("web", "sites", cb); });
        QVERIFY(r.value("ok").toBool() && !r.contains("gitError"));
        QVERIFY(store.exists("sites/example.org"));
        r = call(engine, [&](QJSValue cb) { store.remove("bank", cb); });
        QVERIFY(r.value("ok").toBool() && !r.contains("gitError"));

        const QString storePath = data.filePath("store");
        QCOMPARE(run(storePath, QStringList() << "status" << "--porcelain"), QString());
        QCOMPARE(run(storePath, QStringList() << "rev-list" << "--count" << "HEAD"), QStringLiteral("6"));

        // Sync to an empty bare remote, then clone it as a second phone
        const QString remote = data.filePath("remote.git");
        run(data.path(), QStringList() << "init" << "--bare" << "-q" << remote);
        r = call(engine, [&](QJSValue cb) { git.setRemoteUrl(remote, cb); });
        QVERIFY(r.value("ok").toBool());
        QTRY_COMPARE(git.remoteUrl(), remote);
        r = call(engine, [&](QJSValue cb) { git.sync(cb); });
        QVERIFY2(r.value("ok").toBool(), qPrintable(r.value("error").toString()));

        Git other(data.filePath("other"));
        other.setAuthMode(QStringLiteral("none"));
        other.setAuthorName(QStringLiteral("Other"));
        other.setAuthorEmail(QStringLiteral("other@example.com"));
        PassStore otherStore(data.filePath("other/store"), &pgp, &other);
        r = call(engine, [&](QJSValue cb) { other.clone(remote, cb); });
        QVERIFY2(r.value("ok").toBool(), qPrintable(r.value("error").toString()));
        QVERIFY(otherStore.initialized());
        r = call(engine, [&](QJSValue cb) { otherStore.save("new", "pw4\n", "", cb); });
        QVERIFY(r.value("ok").toBool());
        r = call(engine, [&](QJSValue cb) { other.sync(cb); });
        QVERIFY2(r.value("ok").toBool(), qPrintable(r.value("error").toString()));

        r = call(engine, [&](QJSValue cb) { git.pull(cb); });
        QVERIFY2(r.value("ok").toBool(), qPrintable(r.value("error").toString()));
        QVERIFY(store.exists("new"));

        r = call(engine, [&](QJSValue cb) { git.log(cb); });
        QCOMPARE(r.value("entries").toList().size(), 7);

        // Git operations queue: a save while a sync runs both commit
        engine.globalObject().setProperty("results", engine.newArray());
        const QJSValue collect = engine.evaluate("(function(r) { results.push(r); })");
        git.sync(collect);
        store.save("queued", "pw5\n", "", collect);
        QTRY_COMPARE_WITH_TIMEOUT(engine.evaluate("results.length").toInt(), 2, 30000);
        QVERIFY(engine.evaluate("results[0].ok && results[1].ok && !results[1].gitError").toBool());
        QCOMPARE(run(storePath, QStringList() << "status" << "--porcelain"), QString());

        // Both phones change the same entry: the pull is undone, not left half done
        r = call(engine, [&](QJSValue cb) { otherStore.save("new", "other\n", "new", cb); });
        r = call(engine, [&](QJSValue cb) { other.sync(cb); });
        QVERIFY(r.value("ok").toBool());
        r = call(engine, [&](QJSValue cb) { store.save("new", "mine\n", "new", cb); });
        r = call(engine, [&](QJSValue cb) { git.sync(cb); });
        QVERIFY(!r.value("ok").toBool());
        QVERIFY2(r.value("error").toString().contains("undone"), qPrintable(r.value("error").toString()));
        QVERIFY(!QFileInfo::exists(storePath + "/.git/rebase-merge"));
        QVERIFY(!QFileInfo::exists(storePath + "/.git/rebase-apply"));
        QCOMPARE(run(storePath, QStringList() << "status" << "--porcelain"), QString());
        r = call(engine, [&](QJSValue cb) { store.decrypt("new", cb); });
        QCOMPARE(r.value("text").toString(), QStringLiteral("mine\n"));

        // Symbolic links could lead out of the store
        QDir().mkpath(data.filePath("outside"));
        QFile outsideFile(data.filePath("outside/keep"));
        QVERIFY(outsideFile.open(QIODevice::WriteOnly));
        outsideFile.close();
        QVERIFY(QFile::link(data.filePath("outside"), storePath + "/evil"));
        QVERIFY(!store.validatePath("evil").isEmpty());
        QVERIFY(!store.validatePath("evil/x").isEmpty());
        r = call(engine, [&](QJSValue cb) { store.remove("evil", cb); });
        QVERIFY(!r.value("ok").toBool());
        r = call(engine, [&](QJSValue cb) { store.save("evil/x", "pw\n", "", cb); });
        QVERIFY(!r.value("ok").toBool());
        QVERIFY(QFile::exists(data.filePath("outside/keep")));
        QVERIFY(!QFile::exists(data.filePath("outside/x.gpg")));
        QFile::remove(storePath + "/evil");
    }

    void moveReencrypts()
    {
        QTemporaryDir data;
        QJSEngine engine;
        JsCallback::setEngine(&engine);
        Pgp pgp(data.filePath("keys"));
        Git git(data.path());
        PassStore store(data.filePath("store"), &pgp, &git);

        QVariantMap r = call(engine, [&](QJSValue cb) { pgp.generateKey("Me <me@example.com>", "", cb); });
        const QString mine = r.value("fingerprint").toString();
        r = call(engine, [&](QJSValue cb) { pgp.generateKey("Team <team@example.com>", "", cb); });
        const QString team = r.value("fingerprint").toString();
        // .gpg-id may name keys by short id or by fingerprint in groups
        QString grouped;
        for (int i = 0; i < team.length(); i += 4)
            grouped += team.mid(i, 4) + (i + 4 < team.length() ? " " : "");
        call(engine, [&](QJSValue cb) { store.init("", QStringList() << "0x" + mine.right(8), cb); });
        call(engine, [&](QJSValue cb) { store.init("team", QStringList() << grouped, cb); });
        QCOMPARE(store.recipients("team/x"), QStringList() << grouped);

        r = call(engine, [&](QJSValue cb) { store.save("team/x", "shared\n", "", cb); });
        QVERIFY2(r.value("ok").toBool(), qPrintable(r.value("error").toString()));
        r = call(engine, [&](QJSValue cb) { store.save("team/y", "also shared\n", "", cb); });

        // The folder takes its .gpg-id along, nothing to re-encrypt
        QFile y(data.filePath("store/team/y.gpg"));
        QVERIFY(y.open(QIODevice::ReadOnly));
        const QByteArray before = y.readAll();
        y.close();
        r = call(engine, [&](QJSValue cb) { store.move("team", "groups/team", cb); });
        QVERIFY2(r.value("ok").toBool(), qPrintable(r.value("error").toString()));
        QFile movedY(data.filePath("store/groups/team/y.gpg"));
        QVERIFY(movedY.open(QIODevice::ReadOnly));
        QCOMPARE(movedY.readAll(), before);

        // Out of the team folder, x is re-encrypted for my key only
        r = call(engine, [&](QJSValue cb) { store.move("groups/team/x", "x", cb); });
        QVERIFY2(r.value("ok").toBool(), qPrintable(r.value("error").toString()));
        QVERIFY(store.exists("x"));
        QVERIFY(!QFile::exists(data.filePath("store/x.gpg.reencrypt")));
        r = call(engine, [&](QJSValue cb) { pgp.removeKey(team, cb); });
        r = call(engine, [&](QJSValue cb) { store.decrypt("x", cb); });
        QVERIFY2(r.value("ok").toBool(), qPrintable(r.value("error").toString()));
        QCOMPARE(r.value("text").toString(), QStringLiteral("shared\n"));
        r = call(engine, [&](QJSValue cb) { store.decrypt("groups/team/y", cb); });
        QVERIFY(!r.value("ok").toBool());

        // Without the team key, moving y back can't re-encrypt, so nothing moves
        r = call(engine, [&](QJSValue cb) { store.move("groups/team/y", "y", cb); });
        QVERIFY(!r.value("ok").toBool());
        QVERIFY(store.exists("groups/team/y"));
        QVERIFY(!store.exists("y"));
    }

    void qrCodes()
    {
        if (QStandardPaths::findExecutable(QStringLiteral("qrencode")).isEmpty())
            QSKIP("qrencode not installed");
        const QString uri = QStringLiteral("otpauth://totp/Example:alex@example.com?secret=JBSWY3DPEHPK3PXP&issuer=Example");
        const QString path = m_data.filePath("qr.png");
        QProcess::execute(QStringLiteral("qrencode"), QStringList() << "-s" << "6" << "-o" << path << uri);
        const QImage code(path);
        QVERIFY(!code.isNull());
        QCOMPARE(QrScanner::decode(code), QStringList() << uri);

        // Like a camera frame: turned, smaller, off centre on a grey background
        QImage frame(1080, 1920, QImage::Format_RGB32);
        frame.fill(QColor(90, 90, 90));
        QPainter painter(&frame);
        painter.translate(400, 900);
        painter.rotate(17);
        painter.drawImage(QRect(0, 0, 420, 420), code);
        painter.end();
        QCOMPARE(QrScanner::decode(frame), QStringList() << uri);

        QCOMPARE(QrScanner::decode(QImage(200, 200, QImage::Format_RGB32)), QStringList());
    }

    void askPassHost()
    {
        qputenv("PASSWORD_STORE_ASKPASS", "1");
        qunsetenv("PASSWORD_STORE_SSH");
        qputenv("PASSWORD_STORE_GIT_HOST", "example.com");
        qputenv("PASSWORD_STORE_GIT_PASSWORD", "secret");
        const auto ask = [](const char *prompt) {
            char *argv[] = { const_cast<char *>("password-store"), const_cast<char *>(prompt), nullptr };
            return Git::runHelper(2, argv);
        };
        QCOMPARE(ask("Password for 'https://me@example.com': "), 0);
        QCOMPARE(ask("me@example.com's password: "), 0);
        QCOMPARE(ask("Password for 'https://me@evil.com': "), 1);
        QCOMPARE(ask("Are you sure you want to continue connecting (example.com)?"), 1);
        qunsetenv("PASSWORD_STORE_GIT_HOST");
        QCOMPARE(ask("Password for 'https://me@example.com': "), 1);
        qunsetenv("PASSWORD_STORE_ASKPASS");
        qunsetenv("PASSWORD_STORE_GIT_PASSWORD");
    }
};

QTEST_GUILESS_MAIN(CoreTest)
#include "tst_core.moc"
