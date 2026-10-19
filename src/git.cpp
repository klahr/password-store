#include "git.h"
#include "jscallback.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QVector>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace {

const char *AskPassMarker = "PASSWORD_STORE_ASKPASS";
const char *SshMarker = "PASSWORD_STORE_SSH";
// ssh options, one argument per line
const char *SshOptionsVariable = "PASSWORD_STORE_SSH_OPTIONS";
const char *UsernameVariable = "PASSWORD_STORE_GIT_USERNAME";
const char *PasswordVariable = "PASSWORD_STORE_GIT_PASSWORD";
// Credentials are only given to prompts that name this host
const char *HostVariable = "PASSWORD_STORE_GIT_HOST";

// The last few lines of output say what went wrong
QString errorText(const QString &output)
{
    QStringList lines = output.split(QLatin1Char('\n'), QString::SkipEmptyParts);
    while (lines.size() > 4)
        lines.removeFirst();
    return lines.join(QLatin1Char('\n')).trimmed();
}

// Host of an URL or of scp-like [user@]host:path
QString hostOf(const QString &url)
{
    if (url.contains(QLatin1String("://")))
        return QUrl(url).host();
    const QString hostPart = url.section(QLatin1Char(':'), 0, 0);
    return hostPart.mid(hostPart.lastIndexOf(QLatin1Char('@')) + 1);
}

}

Git::Git(const QString &dataDir, QObject *parent)
    : QObject(parent)
    , m_program(QStandardPaths::findExecutable(QStringLiteral("git")))
    , m_dataDir(dataDir)
    , m_authMode(QStringLiteral("ssh-key"))
    , m_rebaseOnPull(true)
    , m_pending(0)
    , m_running(false)
{
    QDir().mkpath(m_dataDir + QStringLiteral("/ssh"));
}

int Git::runHelper(int argc, char *argv[])
{
    // git runs GIT_SSH with at least the host and a command, askpass gets
    // only the prompt
    if (qgetenv(SshMarker) == "1" && argc >= 3) {
        const QList<QByteArray> options = qgetenv(SshOptionsVariable).split('\n');
        QVector<char *> args;
        args << const_cast<char *>("ssh");
        for (const QByteArray &option : options)
            args << const_cast<char *>(option.constData());
        for (int i = 1; i < argc; ++i)
            args << argv[i];
        args << nullptr;
        // ssh may run this binary again, as askpass
        unsetenv(SshMarker);
        execvp("ssh", args.data());
        fprintf(stderr, "Could not run ssh: %s\n", strerror(errno));
        return 1;
    }
    if (qgetenv(AskPassMarker) == "1") {
        const QByteArray prompt = argc > 1 ? QByteArray(argv[1]) : QByteArray();
        const QByteArray host = qgetenv(HostVariable);
        // Not for another host, like one an https redirect leads to
        if (host.isEmpty() || !prompt.contains(host))
            return 1;
        QByteArray answer;
        if (prompt.startsWith("Username for '"))
            answer = qgetenv(UsernameVariable);
        else if (prompt.contains("assword"))
            answer = qgetenv(PasswordVariable);
        else
            return 1;
        fprintf(stdout, "%s\n", answer.constData());
        return 0;
    }
    return -1;
}

void Git::setStorePath(const QString &path)
{
    if (path == m_storePath)
        return;
    m_storePath = path;
    emit storePathChanged();
    emit isRepoChanged();
    refreshRemoteUrl();
}

bool Git::isRepo() const
{
    return !m_storePath.isEmpty() && QFileInfo(m_storePath + QStringLiteral("/.git")).exists();
}

void Git::setAuthMode(const QString &mode)
{
    if (mode == m_authMode)
        return;
    m_authMode = mode;
    emit settingsChanged();
    emit needsPasswordChanged();
}

void Git::setUsername(const QString &username)
{
    if (username == m_username)
        return;
    m_username = username;
    emit settingsChanged();
}

void Git::setAuthorName(const QString &name)
{
    if (name == m_authorName)
        return;
    m_authorName = name;
    emit settingsChanged();
}

void Git::setAuthorEmail(const QString &email)
{
    if (email == m_authorEmail)
        return;
    m_authorEmail = email;
    emit settingsChanged();
}

void Git::setRebaseOnPull(bool rebase)
{
    if (rebase == m_rebaseOnPull)
        return;
    m_rebaseOnPull = rebase;
    emit settingsChanged();
}

bool Git::needsPassword() const
{
    return m_authMode == QLatin1String("password") && m_password.isEmpty();
}

void Git::setPassword(const QString &password)
{
    m_password = password;
    emit needsPasswordChanged();
}

void Git::forgetPassword()
{
    m_password.fill(QLatin1Char('\0'));
    m_password.clear();
    emit needsPasswordChanged();
}

QString Git::sshKeyPath() const
{
    return m_dataDir + QStringLiteral("/ssh/id_ed25519");
}

bool Git::hasSshKey() const
{
    return QFile::exists(sshKeyPath());
}

QString Git::publicSshKey() const
{
    QFile file(sshKeyPath() + QStringLiteral(".pub"));
    if (!file.open(QIODevice::ReadOnly))
        return QString();
    return QString::fromUtf8(file.readAll()).trimmed();
}

QString Git::remoteUrlDisplay() const
{
    if (!m_remoteUrl.contains(QLatin1String("://")))
        return m_remoteUrl;
    return QUrl(m_remoteUrl).toDisplayString(QUrl::RemovePassword);
}

QString Git::remoteHost() const
{
    return hostOf(m_remoteUrl);
}

QProcessEnvironment Git::environment(const QString &remoteHost) const
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString self = QCoreApplication::applicationFilePath();
    const QString sshDir = m_dataDir + QStringLiteral("/ssh");
    QStringList ssh;
    ssh << QStringLiteral("-F") << QStringLiteral("/dev/null")
        << QStringLiteral("-o") << QStringLiteral("StrictHostKeyChecking=accept-new")
        << QStringLiteral("-o") << QStringLiteral("UserKnownHostsFile=") + sshDir + QStringLiteral("/known_hosts");
    if (m_authMode == QLatin1String("password")) {
        ssh << QStringLiteral("-o") << QStringLiteral("PreferredAuthentications=keyboard-interactive,password")
            << QStringLiteral("-o") << QStringLiteral("NumberOfPasswordPrompts=1");
        env.insert(QStringLiteral("SSH_ASKPASS"), self);
        env.insert(QStringLiteral("SSH_ASKPASS_REQUIRE"), QStringLiteral("force"));
    } else {
        ssh << QStringLiteral("-o") << QStringLiteral("BatchMode=yes");
        if (m_authMode == QLatin1String("ssh-key"))
            ssh << QStringLiteral("-o") << QStringLiteral("IdentitiesOnly=yes") << QStringLiteral("-i") << sshKeyPath();
    }
    // GIT_SSH_COMMAND would need a shell, GIT_SSH is run directly
    env.remove(QStringLiteral("GIT_SSH_COMMAND"));
    env.insert(QStringLiteral("GIT_SSH"), self);
    env.insert(QStringLiteral("GIT_SSH_VARIANT"), QStringLiteral("ssh"));
    env.insert(QString::fromLatin1(SshMarker), QStringLiteral("1"));
    env.insert(QString::fromLatin1(SshOptionsVariable), ssh.join(QLatin1Char('\n')));
    env.insert(QStringLiteral("GIT_TERMINAL_PROMPT"), QStringLiteral("0"));
    // Entry names are paths, not patterns
    env.insert(QStringLiteral("GIT_LITERAL_PATHSPECS"), QStringLiteral("1"));
    // Output is matched against git's English messages
    env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    env.insert(QStringLiteral("GIT_ASKPASS"), self);
    env.insert(QString::fromLatin1(AskPassMarker), QStringLiteral("1"));
    if (!remoteHost.isEmpty()) {
        env.insert(QString::fromLatin1(HostVariable), remoteHost);
        env.insert(QString::fromLatin1(UsernameVariable), m_username);
        env.insert(QString::fromLatin1(PasswordVariable), m_password);
    }
    return env;
}

QStringList Git::configArgs() const
{
    QStringList args;
    args << QStringLiteral("-c") << QStringLiteral("credential.helper=");
    if (!m_authorName.isEmpty())
        args << QStringLiteral("-c") << QStringLiteral("user.name=") + m_authorName;
    if (!m_authorEmail.isEmpty())
        args << QStringLiteral("-c") << QStringLiteral("user.email=") + m_authorEmail;
    return args;
}

void Git::run(const QString &program, const QStringList &args, const QString &workDir,
              const ProcessCallback &callback, const QString &remoteHost)
{
    ++m_pending;
    if (m_pending == 1 && !m_running)
        emit busyChanged();

    QProcess *process = new QProcess(this);
    process->setProcessEnvironment(environment(remoteHost));
    process->setWorkingDirectory(workDir);
    process->setProcessChannelMode(QProcess::MergedChannels);
    process->setStandardInputFile(QProcess::nullDevice());

    auto done = [this, process, callback](int exitCode, const QString &output) {
        process->deleteLater();
        --m_pending;
        if (m_pending == 0 && !m_running)
            emit busyChanged();
        callback(exitCode, output);
    };
    connect(process, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            this, [process, done](int exitCode, QProcess::ExitStatus status) {
        const QString output = QString::fromUtf8(process->readAll());
        done(status == QProcess::NormalExit ? exitCode : -1, output);
    });
    connect(process, &QProcess::errorOccurred, this, [process, program, done](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            done(-1, tr("Could not run %1: %2").arg(program, process->errorString()));
    });
    process->start(program, args);
}

void Git::runGit(const QStringList &args, const ProcessCallback &callback, const QString &workDir,
                 const QString &remoteHost)
{
    if (m_program.isEmpty()) {
        callback(-1, tr("git is not installed"));
        return;
    }
    run(m_program, configArgs() + args, workDir.isEmpty() ? m_storePath : workDir, callback, remoteHost);
}

QVariantMap Git::result(int exitCode, const QString &output)
{
    QVariantMap map;
    map.insert(QStringLiteral("ok"), exitCode == 0);
    if (exitCode != 0)
        map.insert(QStringLiteral("error"), errorText(output));
    return map;
}

Git::Callback Git::jsCallback(QJSValue callback)
{
    return [callback](const QVariantMap &result) { JsCallback::call(callback, result); };
}

void Git::enqueue(const Operation &operation, const Callback &callback)
{
    m_queue.enqueue([this, operation, callback]() {
        operation([this, callback](const QVariantMap &result) {
            if (callback)
                callback(result);
            // Later, so a long queue doesn't grow the stack
            QTimer::singleShot(0, this, &Git::runNext);
        });
    });
    if (!m_running)
        runNext();
}

void Git::runNext()
{
    const bool wasBusy = busy();
    m_running = !m_queue.isEmpty();
    if (busy() != wasBusy)
        emit busyChanged();
    if (m_running)
        m_queue.dequeue()();
}

void Git::refreshRemoteUrl()
{
    enqueue([this](const Callback &done) {
        if (!isRepo()) {
            done(result(0, QString()));
            return;
        }
        runGit(QStringList() << QStringLiteral("config") << QStringLiteral("--get")
                             << QStringLiteral("remote.origin.url"),
               [done](int exitCode, const QString &output) {
            QVariantMap map = result(0, QString());
            map.insert(QStringLiteral("url"), exitCode == 0 ? output.trimmed() : QString());
            done(map);
        });
    }, [this](const QVariantMap &result) {
        const QString url = result.value(QStringLiteral("url")).toString();
        if (url != m_remoteUrl) {
            m_remoteUrl = url;
            emit remoteUrlChanged();
        }
    });
}

void Git::clone(const QString &url, QJSValue callback)
{
    enqueue([this, url](const Callback &done) {
        QDir dir(m_storePath);
        if (dir.exists() && !dir.entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot).isEmpty()) {
            done(result(1, tr("The store folder %1 is not empty").arg(m_storePath)));
            return;
        }
        const QString parent = QFileInfo(m_storePath).absolutePath();
        QDir().mkpath(parent);
        runGit(QStringList() << QStringLiteral("clone") << QStringLiteral("--") << url << m_storePath,
               [this, done](int exitCode, const QString &output) {
            emit isRepoChanged();
            emit storeChanged();
            done(result(exitCode, output));
        }, parent, hostOf(url));
    }, jsCallback(callback));
    refreshRemoteUrl();
}

void Git::init(QJSValue callback)
{
    enqueue([this](const Callback &done) {
        QDir().mkpath(m_storePath);
        runGit(QStringList() << QStringLiteral("init"), [this, done](int exitCode, const QString &output) {
            emit isRepoChanged();
            if (exitCode != 0) {
                done(result(exitCode, output));
                return;
            }
            doCommit(QStringList() << QStringLiteral("."), tr("Initialize password store"), done);
        });
    }, jsCallback(callback));
}

void Git::commit(const QStringList &paths, const QString &message, const Callback &callback)
{
    enqueue([this, paths, message](const Callback &done) { doCommit(paths, message, done); }, callback);
}

void Git::commit(const QStringList &paths, const QString &message, QJSValue callback)
{
    commit(paths, message, jsCallback(callback));
}

void Git::doCommit(const QStringList &paths, const QString &message, const Callback &callback)
{
    if (!isRepo()) {
        callback(result(0, QString()));
        return;
    }
    runGit(QStringList() << QStringLiteral("add") << QStringLiteral("-A") << QStringLiteral("--") << paths,
           [this, message, callback](int exitCode, const QString &output) {
        // Folders git never tracked, like empty ones, have nothing to commit
        if (exitCode != 0 && output.contains(QLatin1String("did not match any files"))) {
            callback(result(0, QString()));
            return;
        }
        if (exitCode != 0) {
            callback(result(exitCode, output));
            return;
        }
        runGit(QStringList() << QStringLiteral("commit") << QStringLiteral("-q") << QStringLiteral("-m") << message,
               [callback](int exitCode, const QString &output) {
            // Nothing staged isn't an error
            if (exitCode == 1 && (output.contains(QLatin1String("nothing to commit"))
                                  || output.contains(QLatin1String("nothing added to commit"))))
                exitCode = 0;
            QVariantMap map = result(exitCode, output);
            if (output.contains(QLatin1String("Author identity unknown")))
                map.insert(QStringLiteral("error"), tr("Set the git author name and email in settings"));
            callback(map);
        });
    });
}

void Git::withUpstream(const std::function<void(bool hasUpstream)> &next)
{
    runGit(QStringList() << QStringLiteral("rev-parse") << QStringLiteral("--abbrev-ref")
                         << QStringLiteral("--symbolic-full-name") << QStringLiteral("@{u}"),
           [next](int exitCode, const QString &) { next(exitCode == 0); });
}

void Git::doPull(const Callback &callback)
{
    withUpstream([this, callback](bool hasUpstream) {
        QStringList args;
        args << QStringLiteral("pull") << (m_rebaseOnPull ? QStringLiteral("--rebase") : QStringLiteral("--no-rebase"));
        if (!hasUpstream)
            args << QStringLiteral("origin") << QStringLiteral("HEAD");
        runGit(args, [this, callback](int exitCode, const QString &output) {
            if (exitCode == 0) {
                emit storeChanged();
                callback(result(0, output));
                return;
            }
            // Don't leave the store in the middle of a rebase or merge
            const QString gitDir = m_storePath + QStringLiteral("/.git/");
            QStringList abort;
            if (QFileInfo(gitDir + QStringLiteral("rebase-merge")).exists()
                || QFileInfo(gitDir + QStringLiteral("rebase-apply")).exists())
                abort << QStringLiteral("rebase") << QStringLiteral("--abort");
            else if (QFileInfo(gitDir + QStringLiteral("MERGE_HEAD")).exists())
                abort << QStringLiteral("merge") << QStringLiteral("--abort");
            const QVariantMap failed = result(exitCode, output);
            if (abort.isEmpty()) {
                callback(failed);
                return;
            }
            runGit(abort, [this, callback, failed](int, const QString &) {
                emit storeChanged();
                QVariantMap map = failed;
                map.insert(QStringLiteral("error"),
                           tr("Changes on the server conflict with yours, the pull was undone:\n%1")
                               .arg(failed.value(QStringLiteral("error")).toString()));
                callback(map);
            });
        }, QString(), remoteHost());
    });
}

void Git::doPush(const Callback &callback)
{
    withUpstream([this, callback](bool hasUpstream) {
        QStringList args;
        args << QStringLiteral("push");
        if (!hasUpstream)
            args << QStringLiteral("--set-upstream") << QStringLiteral("origin") << QStringLiteral("HEAD");
        runGit(args, [callback](int exitCode, const QString &output) { callback(result(exitCode, output)); },
               QString(), remoteHost());
    });
}

void Git::pull(QJSValue callback)
{
    enqueue([this](const Callback &done) { doPull(done); }, jsCallback(callback));
}

void Git::push(QJSValue callback)
{
    enqueue([this](const Callback &done) { doPush(done); }, jsCallback(callback));
}

void Git::sync(QJSValue callback)
{
    enqueue([this](const Callback &done) {
        doCommit(QStringList() << QStringLiteral("."), tr("Sync"), [this, done](const QVariantMap &committed) {
            if (!committed.value(QStringLiteral("ok")).toBool()) {
                done(committed);
                return;
            }
            withUpstream([this, done](bool hasUpstream) {
                // A new store has nothing to pull yet
                if (!hasUpstream) {
                    doPush(done);
                    return;
                }
                doPull([this, done](const QVariantMap &pulled) {
                    if (!pulled.value(QStringLiteral("ok")).toBool())
                        done(pulled);
                    else
                        doPush(done);
                });
            });
        });
    }, jsCallback(callback));
}

void Git::log(QJSValue callback)
{
    enqueue([this](const Callback &done) {
        runGit(QStringList() << QStringLiteral("log") << QStringLiteral("-n") << QStringLiteral("200")
                             << QStringLiteral("--format=%H%x1f%an%x1f%at%x1f%s%x1e"),
               [done](int exitCode, const QString &output) {
            QVariantMap map = result(exitCode, output);
            QVariantList entries;
            if (exitCode == 0) {
                for (const QString &record : output.split(QChar(0x1e), QString::SkipEmptyParts)) {
                    const QStringList parts = record.trimmed().split(QChar(0x1f));
                    if (parts.size() < 4)
                        continue;
                    QVariantMap entry;
                    entry.insert(QStringLiteral("hash"), parts.at(0));
                    entry.insert(QStringLiteral("author"), parts.at(1));
                    entry.insert(QStringLiteral("time"), QDateTime::fromMSecsSinceEpoch(parts.at(2).toLongLong() * 1000));
                    entry.insert(QStringLiteral("message"), parts.at(3));
                    entries << entry;
                }
            }
            map.insert(QStringLiteral("entries"), entries);
            done(map);
        });
    }, jsCallback(callback));
}

void Git::setRemoteUrl(const QString &url, QJSValue callback)
{
    enqueue([this, url](const Callback &done) {
        runGit(QStringList() << QStringLiteral("remote") << QStringLiteral("get-url") << QStringLiteral("origin"),
               [this, url, done](int exitCode, const QString &) {
            runGit(QStringList() << QStringLiteral("remote")
                                 << (exitCode == 0 ? QStringLiteral("set-url") : QStringLiteral("add"))
                                 << QStringLiteral("--") << QStringLiteral("origin") << url,
                   [done](int exitCode, const QString &output) { done(result(exitCode, output)); });
        });
    }, jsCallback(callback));
    refreshRemoteUrl();
}

void Git::generateSshKey(QJSValue callback)
{
    const Callback cb = jsCallback(callback);
    const QString path = sshKeyPath();
    const QString newPath = path + QStringLiteral(".new");
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile::remove(newPath);
    QFile::remove(newPath + QStringLiteral(".pub"));
    run(QStringLiteral("ssh-keygen"),
        QStringList() << QStringLiteral("-q") << QStringLiteral("-t") << QStringLiteral("ed25519")
                      << QStringLiteral("-N") << QString() << QStringLiteral("-C")
                      << QStringLiteral("password-store@sailfishos") << QStringLiteral("-f") << newPath,
        m_dataDir, [this, cb, path, newPath](int exitCode, const QString &output) {
        // The old key stays until the new one exists
        if (exitCode == 0) {
            ::rename(QFile::encodeName(newPath + QStringLiteral(".pub")).constData(),
                     QFile::encodeName(path + QStringLiteral(".pub")).constData());
            ::rename(QFile::encodeName(newPath).constData(), QFile::encodeName(path).constData());
        } else {
            QFile::remove(newPath);
            QFile::remove(newPath + QStringLiteral(".pub"));
        }
        emit sshKeyChanged();
        cb(result(exitCode, output));
    });
}

void Git::importSshKey(const QString &path, QJSValue callback)
{
    const Callback cb = jsCallback(callback);
    const QString target = sshKeyPath();
    const QString newPath = target + QStringLiteral(".new");
    QDir().mkpath(QFileInfo(target).absolutePath());
    QFile::remove(newPath);
    if (!QFile::copy(path, newPath)) {
        cb(result(1, tr("Could not read %1").arg(path)));
        return;
    }
    QFile::setPermissions(newPath, QFile::ReadOwner | QFile::WriteOwner);
    // Deriving the public key also checks the private key is usable without a passphrase
    run(QStringLiteral("ssh-keygen"),
        QStringList() << QStringLiteral("-y") << QStringLiteral("-P") << QString() << QStringLiteral("-f") << newPath,
        m_dataDir, [this, cb, target, newPath](int exitCode, const QString &output) {
        QString publicKey;
        for (const QString &line : output.split(QLatin1Char('\n'))) {
            if (line.startsWith(QLatin1String("ssh-")) || line.startsWith(QLatin1String("ecdsa-"))
                || line.startsWith(QLatin1String("sk-")))
                publicKey = line.trimmed();
        }
        if (exitCode != 0 || publicKey.isEmpty()) {
            QFile::remove(newPath);
            cb(result(1, tr("Not a private key without a passphrase")));
            return;
        }
        QFile pub(target + QStringLiteral(".pub"));
        if (pub.open(QIODevice::WriteOnly | QIODevice::Truncate))
            pub.write(publicKey.toUtf8() + '\n');
        ::rename(QFile::encodeName(newPath).constData(), QFile::encodeName(target).constData());
        emit sshKeyChanged();
        cb(result(0, QString()));
    });
}

void Git::removeSshKey()
{
    QFile::remove(sshKeyPath());
    QFile::remove(sshKeyPath() + QStringLiteral(".pub"));
    emit sshKeyChanged();
}
