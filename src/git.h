#ifndef GIT_H
#define GIT_H

#include <QJSValue>
#include <QObject>
#include <QQueue>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <functional>

class QProcessEnvironment;

// Runs the system git, with ssh using an app-private key and known_hosts.
// The sandbox has no shell, so this binary stands in for what git would run
// through one: it starts ssh with the app's options and answers password
// prompts as askpass helper.
class Git : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString storePath READ storePath WRITE setStorePath NOTIFY storePathChanged)
    Q_PROPERTY(bool isRepo READ isRepo NOTIFY isRepoChanged)
    Q_PROPERTY(QString authMode READ authMode WRITE setAuthMode NOTIFY settingsChanged)
    Q_PROPERTY(QString username READ username WRITE setUsername NOTIFY settingsChanged)
    Q_PROPERTY(QString authorName READ authorName WRITE setAuthorName NOTIFY settingsChanged)
    Q_PROPERTY(QString authorEmail READ authorEmail WRITE setAuthorEmail NOTIFY settingsChanged)
    Q_PROPERTY(bool rebaseOnPull READ rebaseOnPull WRITE setRebaseOnPull NOTIFY settingsChanged)
    Q_PROPERTY(bool needsPassword READ needsPassword NOTIFY needsPasswordChanged)
    Q_PROPERTY(bool hasSshKey READ hasSshKey NOTIFY sshKeyChanged)
    Q_PROPERTY(QString publicSshKey READ publicSshKey NOTIFY sshKeyChanged)
    Q_PROPERTY(QString remoteUrl READ remoteUrl NOTIFY remoteUrlChanged)
    // remoteUrl without a password in it
    Q_PROPERTY(QString remoteUrlDisplay READ remoteUrlDisplay NOTIFY remoteUrlChanged)

public:
    typedef std::function<void(const QVariantMap &result)> Callback;

    Git(const QString &dataDir, QObject *parent = nullptr);

    // Handles being run as GIT_SSH, GIT_ASKPASS or SSH_ASKPASS. Returns the
    // exit code, or -1 when this is the app itself.
    static int runHelper(int argc, char *argv[]);

    bool available() const { return !m_program.isEmpty(); }
    bool busy() const { return m_running || m_pending > 0; }
    QString storePath() const { return m_storePath; }
    void setStorePath(const QString &path);
    bool isRepo() const;
    QString authMode() const { return m_authMode; }
    void setAuthMode(const QString &mode);
    QString username() const { return m_username; }
    void setUsername(const QString &username);
    QString authorName() const { return m_authorName; }
    void setAuthorName(const QString &name);
    QString authorEmail() const { return m_authorEmail; }
    void setAuthorEmail(const QString &email);
    bool rebaseOnPull() const { return m_rebaseOnPull; }
    void setRebaseOnPull(bool rebase);
    bool needsPassword() const;
    bool hasSshKey() const;
    QString publicSshKey() const;
    QString remoteUrl() const { return m_remoteUrl; }
    QString remoteUrlDisplay() const;

    // Stages paths and commits them, does nothing outside a git repository.
    // Git operations run one at a time, in the order they were asked for.
    void commit(const QStringList &paths, const QString &message, const Callback &callback);

    // All callback results have ok and error
    Q_INVOKABLE void clone(const QString &url, QJSValue callback);
    Q_INVOKABLE void init(QJSValue callback);
    Q_INVOKABLE void pull(QJSValue callback);
    Q_INVOKABLE void push(QJSValue callback);
    Q_INVOKABLE void sync(QJSValue callback);
    Q_INVOKABLE void commit(const QStringList &paths, const QString &message, QJSValue callback);
    // Result: entries [{hash, author, time, message}]
    Q_INVOKABLE void log(QJSValue callback);
    Q_INVOKABLE void setRemoteUrl(const QString &url, QJSValue callback);
    Q_INVOKABLE void setPassword(const QString &password);
    Q_INVOKABLE void forgetPassword();
    Q_INVOKABLE void generateSshKey(QJSValue callback);
    Q_INVOKABLE void importSshKey(const QString &path, QJSValue callback);
    Q_INVOKABLE void removeSshKey();

signals:
    void busyChanged();
    void storePathChanged();
    void isRepoChanged();
    void settingsChanged();
    void needsPasswordChanged();
    void sshKeyChanged();
    void remoteUrlChanged();
    void storeChanged();

private:
    typedef std::function<void(int exitCode, const QString &output)> ProcessCallback;
    typedef std::function<void(const Callback &done)> Operation;

    // Network operations get the credentials, for the host of remoteHost
    void runGit(const QStringList &args, const ProcessCallback &callback, const QString &workDir = QString(),
                const QString &remoteHost = QString());
    void run(const QString &program, const QStringList &args, const QString &workDir,
             const ProcessCallback &callback, const QString &remoteHost = QString());
    QProcessEnvironment environment(const QString &remoteHost) const;
    QStringList configArgs() const;
    // Runs operation after the ones before it, callback gets its result
    void enqueue(const Operation &operation, const Callback &callback);
    void runNext();
    void refreshRemoteUrl();
    void withUpstream(const std::function<void(bool hasUpstream)> &next);
    void doCommit(const QStringList &paths, const QString &message, const Callback &callback);
    void doPull(const Callback &callback);
    void doPush(const Callback &callback);
    QString remoteHost() const;
    QString sshKeyPath() const;
    static Callback jsCallback(QJSValue callback);
    static QVariantMap result(int exitCode, const QString &output);

    QString m_program;
    QString m_dataDir;
    QString m_storePath;
    QString m_authMode;
    QString m_username;
    QString m_authorName;
    QString m_authorEmail;
    bool m_rebaseOnPull;
    QString m_password;
    QString m_remoteUrl;
    int m_pending;
    bool m_running;
    QQueue<std::function<void()>> m_queue;
};

#endif // GIT_H
