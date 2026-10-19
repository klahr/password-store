#ifndef PGP_H
#define PGP_H

#include <QAtomicInt>
#include <QByteArray>
#include <QJSValue>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QThreadPool>
#include <QTimer>
#include <QVariantMap>

#include <functional>

struct rnp_ffi_st;
struct rnp_key_handle_st;

// OpenPGP through rnp with an app-private keyring. rnp isn't thread safe, so
// every operation runs in order on one worker thread and reports back on the
// main thread.
class Pgp : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool hasPassphrase READ hasPassphrase NOTIFY hasPassphraseChanged)
    Q_PROPERTY(int passphraseTimeout READ passphraseTimeout WRITE setPassphraseTimeout NOTIFY passphraseTimeoutChanged)
    Q_PROPERTY(int secretKeyCount READ secretKeyCount NOTIFY keysChanged)

public:
    typedef std::function<void(const QVariantMap &result)> Callback;

    explicit Pgp(const QString &keyringDir, QObject *parent = nullptr);
    ~Pgp();

    bool busy() const { return m_pending > 0; }
    bool hasPassphrase() const;
    int passphraseTimeout() const { return m_passphraseTimeout; }
    void setPassphraseTimeout(int minutes);
    int secretKeyCount() const { return m_secretKeyCount.load(); }

    // Callback result: ok, text, error, needPassphrase, keyName
    void decrypt(const QString &path, const Callback &callback);
    // Callback result: ok, error, missing (recipients without a usable key)
    void encrypt(const QString &path, const QStringList &recipients, const QString &text,
                 const Callback &callback);

    Q_INVOKABLE void decrypt(const QString &path, QJSValue callback);
    Q_INVOKABLE void setPassphrase(const QString &passphrase);
    Q_INVOKABLE void forgetPassphrase();
    // Callback result: ok, keys [{fingerprint, keyId, uid, uids, secret, algorithm, usable}]
    Q_INVOKABLE void listKeys(QJSValue callback);
    // Imports keys from a file or armored text, which may be encrypted with
    // a passphrase, like gpg --symmetric does.
    // Callback result: ok, error, imported (number of keys), needPassphrase
    Q_INVOKABLE void importKeys(const QString &pathOrArmor, const QString &passphrase, QJSValue callback);
    Q_INVOKABLE void generateKey(const QString &userId, const QString &passphrase, QJSValue callback);
    // Callback result: ok, error, armor
    Q_INVOKABLE void exportPublicKey(const QString &fingerprint, QJSValue callback);
    Q_INVOKABLE void removeKey(const QString &fingerprint, QJSValue callback);

signals:
    void busyChanged();
    void hasPassphraseChanged();
    void passphraseTimeoutChanged();
    void keysChanged();

private:
    typedef std::function<QVariantMap()> Job;

    void run(const Job &job, const Callback &callback);
    static Callback jsCallback(QJSValue callback);

    // Worker thread only
    bool ensureLoaded(QString *error);
    bool saveKeys(QString *error);
    void updateSecretKeyCount();
    QString findKeyFingerprint(const QString &recipient) const;
    QByteArray cachedPassphrase() const;
    static bool passwordProvider(rnp_ffi_st *ffi, void *context, rnp_key_handle_st *key,
                                 const char *pgpContext, char buf[], size_t bufLen);

    QString m_keyringDir;
    QThreadPool m_pool;
    int m_pending;
    rnp_ffi_st *m_ffi;
    bool m_loaded;
    QAtomicInt m_secretKeyCount;

    mutable QMutex m_passphraseMutex;
    QByteArray m_passphrase;
    // Set by the password provider when it had nothing to give
    QString m_askedKey;
    bool m_providerAsked;
    // Set by the password provider when it gave the cached passphrase
    bool m_providerGave;
    // Passphrase for a symmetrically encrypted key file being imported
    QByteArray m_importPassphrase;
    QTimer m_passphraseTimer;
    int m_passphraseTimeout;
};

#endif // PGP_H
