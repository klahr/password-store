#include "pgp.h"
#include "jscallback.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <QtConcurrent>

#include <rnp/rnp.h>
#include <rnp/rnp_err.h>

#include <cstring>
#include <fcntl.h>
#include <unistd.h>

namespace {

const char *KeyringFormat = "GPG";

QString takeString(char *str)
{
    QString result = QString::fromUtf8(str);
    rnp_buffer_destroy(str);
    return result;
}

QString rnpError(rnp_result_t result)
{
    return QString::fromUtf8(rnp_result_to_string(result));
}

QVariantMap failure(const QString &error)
{
    QVariantMap result;
    result.insert(QStringLiteral("ok"), false);
    result.insert(QStringLiteral("error"), error);
    return result;
}

QVariantMap success()
{
    QVariantMap result;
    result.insert(QStringLiteral("ok"), true);
    return result;
}

bool isHex(const QString &str)
{
    for (const QChar c : str) {
        if (!((c >= QLatin1Char('0') && c <= QLatin1Char('9'))
              || (c.toLower() >= QLatin1Char('a') && c.toLower() <= QLatin1Char('f'))))
            return false;
    }
    return !str.isEmpty();
}

// Writes to a temporary file next to path and moves it into place
class AtomicOutput
{
public:
    explicit AtomicOutput(const QString &path)
        : m_path(path)
        , m_tmpPath(path + QStringLiteral(".tmp"))
        , m_output(nullptr)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile::remove(m_tmpPath);
        rnp_output_to_path(&m_output, QFile::encodeName(m_tmpPath).constData());
    }
    ~AtomicOutput()
    {
        if (m_output)
            rnp_output_destroy(m_output);
        QFile::remove(m_tmpPath);
    }
    rnp_output_t handle() const { return m_output; }
    bool commit()
    {
        rnp_output_destroy(m_output);
        m_output = nullptr;
        // On disk before it replaces the old file, which rename() does in one step
        const QByteArray tmpPath = QFile::encodeName(m_tmpPath);
        const int fd = ::open(tmpPath.constData(), O_RDONLY);
        if (fd < 0)
            return false;
        const bool synced = ::fsync(fd) == 0;
        ::close(fd);
        return synced && ::rename(tmpPath.constData(), QFile::encodeName(m_path).constData()) == 0;
    }

private:
    QString m_path;
    QString m_tmpPath;
    rnp_output_t m_output;
};

}

Pgp::Pgp(const QString &keyringDir, QObject *parent)
    : QObject(parent)
    , m_keyringDir(keyringDir)
    , m_pending(0)
    , m_ffi(nullptr)
    , m_loaded(false)
    , m_secretKeyCount(0)
    , m_providerAsked(false)
    , m_providerGave(false)
    , m_passphraseTimeout(5)
{
    m_pool.setMaxThreadCount(1);
    m_passphraseTimer.setSingleShot(true);
    connect(&m_passphraseTimer, &QTimer::timeout, this, &Pgp::forgetPassphrase);

    // Load the keyring right away so the key count is known
    run([this]() {
        QString error;
        if (!ensureLoaded(&error))
            return failure(error);
        return success();
    }, [this](const QVariantMap &) { emit keysChanged(); });
}

Pgp::~Pgp()
{
    m_pool.waitForDone();
    forgetPassphrase();
    if (m_ffi)
        rnp_ffi_destroy(m_ffi);
}

bool Pgp::hasPassphrase() const
{
    QMutexLocker locker(&m_passphraseMutex);
    return !m_passphrase.isEmpty();
}

void Pgp::setPassphraseTimeout(int minutes)
{
    if (minutes == m_passphraseTimeout)
        return;
    m_passphraseTimeout = minutes;
    if (m_passphraseTimeout <= 0)
        m_passphraseTimer.stop();
    else if (hasPassphrase())
        m_passphraseTimer.start(m_passphraseTimeout * 60 * 1000);
    emit passphraseTimeoutChanged();
}

void Pgp::setPassphrase(const QString &passphrase)
{
    {
        QMutexLocker locker(&m_passphraseMutex);
        m_passphrase.fill('\0');
        m_passphrase = passphrase.toUtf8();
    }
    if (m_passphraseTimeout > 0)
        m_passphraseTimer.start(m_passphraseTimeout * 60 * 1000);
    emit hasPassphraseChanged();
}

void Pgp::forgetPassphrase()
{
    m_passphraseTimer.stop();
    {
        QMutexLocker locker(&m_passphraseMutex);
        if (m_passphrase.isEmpty())
            return;
        m_passphrase.fill('\0');
        m_passphrase.clear();
    }
    emit hasPassphraseChanged();
}

QByteArray Pgp::cachedPassphrase() const
{
    QMutexLocker locker(&m_passphraseMutex);
    return m_passphrase;
}

bool Pgp::passwordProvider(rnp_ffi_st *, void *context, rnp_key_handle_st *key,
                           const char *pgpContext, char buf[], size_t bufLen)
{
    Pgp *self = static_cast<Pgp *>(context);
    self->m_providerAsked = true;

    if (pgpContext && strcmp(pgpContext, "decrypt (symmetric)") == 0) {
        const QByteArray &passphrase = self->m_importPassphrase;
        if (passphrase.isEmpty() || size_t(passphrase.size()) >= bufLen)
            return false;
        memcpy(buf, passphrase.constData(), passphrase.size());
        buf[passphrase.size()] = '\0';
        return true;
    }

    char *uid = nullptr;
    if (key && rnp_key_get_primary_uid(key, &uid) == RNP_SUCCESS) {
        self->m_askedKey = takeString(uid);
    } else if (key) {
        // Subkeys have no user id, name them after their primary key
        char *primaryFpr = nullptr;
        rnp_key_handle_t primary = nullptr;
        if (rnp_key_get_primary_fprint(key, &primaryFpr) == RNP_SUCCESS) {
            if (rnp_locate_key(self->m_ffi, "fingerprint", primaryFpr, &primary) == RNP_SUCCESS
                && primary && rnp_key_get_primary_uid(primary, &uid) == RNP_SUCCESS)
                self->m_askedKey = takeString(uid);
            rnp_key_handle_destroy(primary);
            rnp_buffer_destroy(primaryFpr);
        }
    }

    QByteArray passphrase = self->cachedPassphrase();
    if (passphrase.isEmpty() || size_t(passphrase.size()) >= bufLen)
        return false;
    memcpy(buf, passphrase.constData(), passphrase.size());
    buf[passphrase.size()] = '\0';
    passphrase.fill('\0');
    self->m_providerGave = true;
    return true;
}

bool Pgp::ensureLoaded(QString *error)
{
    if (m_loaded)
        return true;

    if (!m_ffi) {
        rnp_result_t result = rnp_ffi_create(&m_ffi, KeyringFormat, KeyringFormat);
        if (result != RNP_SUCCESS) {
            *error = rnpError(result);
            m_ffi = nullptr;
            return false;
        }
        rnp_ffi_set_pass_provider(m_ffi, &Pgp::passwordProvider, this);
    }

    const QPair<QString, uint32_t> rings[] = {
        qMakePair(QStringLiteral("pubring.gpg"), uint32_t(RNP_LOAD_SAVE_PUBLIC_KEYS)),
        qMakePair(QStringLiteral("secring.gpg"), uint32_t(RNP_LOAD_SAVE_SECRET_KEYS)),
    };
    for (const auto &ring : rings) {
        const QString path = m_keyringDir + QLatin1Char('/') + ring.first;
        if (!QFile::exists(path))
            continue;
        rnp_input_t input = nullptr;
        rnp_result_t result = rnp_input_from_path(&input, QFile::encodeName(path).constData());
        if (result == RNP_SUCCESS)
            result = rnp_load_keys(m_ffi, KeyringFormat, input, ring.second);
        rnp_input_destroy(input);
        if (result != RNP_SUCCESS) {
            *error = tr("Could not load %1: %2").arg(ring.first, rnpError(result));
            return false;
        }
    }
    m_loaded = true;
    updateSecretKeyCount();
    return true;
}

void Pgp::updateSecretKeyCount()
{
    size_t count = 0;
    rnp_identifier_iterator_t it = nullptr;
    if (rnp_identifier_iterator_create(m_ffi, &it, "fingerprint") == RNP_SUCCESS) {
        const char *fpr = nullptr;
        while (rnp_identifier_iterator_next(it, &fpr) == RNP_SUCCESS && fpr) {
            rnp_key_handle_t key = nullptr;
            bool primary = false;
            bool secret = false;
            if (rnp_locate_key(m_ffi, "fingerprint", fpr, &key) == RNP_SUCCESS && key
                && rnp_key_is_primary(key, &primary) == RNP_SUCCESS && primary
                && rnp_key_have_secret(key, &secret) == RNP_SUCCESS && secret)
                ++count;
            rnp_key_handle_destroy(key);
        }
        rnp_identifier_iterator_destroy(it);
    }
    m_secretKeyCount.store(int(count));
}

bool Pgp::saveKeys(QString *error)
{
    QDir().mkpath(m_keyringDir);
    QFile::setPermissions(m_keyringDir, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);

    const QPair<QString, uint32_t> rings[] = {
        qMakePair(QStringLiteral("pubring.gpg"), uint32_t(RNP_LOAD_SAVE_PUBLIC_KEYS)),
        qMakePair(QStringLiteral("secring.gpg"), uint32_t(RNP_LOAD_SAVE_SECRET_KEYS)),
    };
    for (const auto &ring : rings) {
        AtomicOutput output(m_keyringDir + QLatin1Char('/') + ring.first);
        rnp_result_t result = rnp_save_keys(m_ffi, KeyringFormat, output.handle(), ring.second);
        if (result != RNP_SUCCESS || !output.commit()) {
            *error = tr("Could not save %1: %2").arg(ring.first, rnpError(result));
            return false;
        }
    }
    updateSecretKeyCount();
    return true;
}

QString Pgp::findKeyFingerprint(const QString &recipient) const
{
    QString id = recipient.trimmed();
    if (id.startsWith(QLatin1String("0x"), Qt::CaseInsensitive))
        id = id.mid(2);
    // Fingerprints are often written in groups
    QString compact = id;
    compact.remove(QLatin1Char(' '));

    // Primary keys and subkeys matching id, valid ones first
    QStringList valid;
    QStringList other;
    const bool hexId = isHex(compact) && (compact.length() == 8 || compact.length() == 16
                                          || compact.length() == 40 || compact.length() == 64);
    rnp_identifier_iterator_t it = nullptr;
    if (rnp_identifier_iterator_create(m_ffi, &it, "fingerprint") != RNP_SUCCESS)
        return QString();
    const char *fpr = nullptr;
    while (rnp_identifier_iterator_next(it, &fpr) == RNP_SUCCESS && fpr) {
        rnp_key_handle_t key = nullptr;
        if (rnp_locate_key(m_ffi, "fingerprint", fpr, &key) != RNP_SUCCESS || !key)
            continue;
        const QString fingerprint = QString::fromLatin1(fpr);
        bool matches = false;
        if (hexId) {
            // Key ids, short ones too, are the end of the fingerprint
            matches = fingerprint.endsWith(compact, Qt::CaseInsensitive);
        } else {
            // Like gpg, anything else matches a substring of a user id
            bool primary = false;
            rnp_key_is_primary(key, &primary);
            size_t count = 0;
            if (primary)
                rnp_key_get_uid_count(key, &count);
            for (size_t i = 0; i < count && !matches; ++i) {
                char *uid = nullptr;
                if (rnp_key_get_uid_at(key, i, &uid) == RNP_SUCCESS)
                    matches = takeString(uid).contains(id, Qt::CaseInsensitive);
            }
        }
        if (matches) {
            bool isValid = false;
            rnp_key_is_valid(key, &isValid);
            (isValid ? valid : other) << fingerprint;
        }
        rnp_key_handle_destroy(key);
    }
    rnp_identifier_iterator_destroy(it);
    if (!valid.isEmpty())
        return valid.first();
    return other.isEmpty() ? QString() : other.first();
}

void Pgp::run(const Job &job, const Callback &callback)
{
    ++m_pending;
    if (m_pending == 1)
        emit busyChanged();

    QFutureWatcher<QVariantMap> *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this, [this, watcher, callback]() {
        const QVariantMap result = watcher->result();
        watcher->deleteLater();
        --m_pending;
        if (m_pending == 0)
            emit busyChanged();
        if (callback)
            callback(result);
    });
    watcher->setFuture(QtConcurrent::run(&m_pool, job));
}

Pgp::Callback Pgp::jsCallback(QJSValue callback)
{
    return [callback](const QVariantMap &result) { JsCallback::call(callback, result); };
}

void Pgp::decrypt(const QString &path, const Callback &callback)
{
    run([this, path]() {
        QString error;
        if (!ensureLoaded(&error))
            return failure(error);

        m_providerAsked = false;
        m_providerGave = false;
        m_askedKey.clear();

        rnp_input_t input = nullptr;
        rnp_output_t output = nullptr;
        rnp_result_t result = rnp_input_from_path(&input, QFile::encodeName(path).constData());
        if (result == RNP_SUCCESS)
            result = rnp_output_to_memory(&output, 0);
        if (result == RNP_SUCCESS)
            result = rnp_decrypt(m_ffi, input, output);

        QVariantMap map;
        if (result == RNP_SUCCESS) {
            uint8_t *buf = nullptr;
            size_t len = 0;
            rnp_output_memory_get_buf(output, &buf, &len, false);
            map = success();
            map.insert(QStringLiteral("text"), QString::fromUtf8(reinterpret_cast<const char *>(buf), int(len)));
            rnp_buffer_clear(buf, len);
        } else if (m_providerAsked && !m_providerGave) {
            map = failure(QString());
            map.insert(QStringLiteral("needPassphrase"), true);
            map.insert(QStringLiteral("keyName"), m_askedKey);
        } else if (m_providerGave && (result == RNP_ERROR_BAD_PASSWORD || result == RNP_ERROR_NO_SUITABLE_KEY)) {
            map = failure(tr("Wrong passphrase"));
            map.insert(QStringLiteral("needPassphrase"), true);
            map.insert(QStringLiteral("keyName"), m_askedKey);
        } else if (result == RNP_ERROR_NO_SUITABLE_KEY || result == RNP_ERROR_KEY_NOT_FOUND) {
            map = failure(tr("None of your keys can decrypt this entry"));
        } else {
            map = failure(rnpError(result));
        }
        rnp_output_destroy(output);
        rnp_input_destroy(input);
        return map;
    }, [this, callback](const QVariantMap &result) {
        // A wrong passphrase shouldn't be offered again
        if (result.value(QStringLiteral("needPassphrase")).toBool())
            forgetPassphrase();
        callback(result);
    });
}

void Pgp::decrypt(const QString &path, QJSValue callback)
{
    decrypt(path, jsCallback(callback));
}

void Pgp::encrypt(const QString &path, const QStringList &recipients, const QString &text,
                  const Callback &callback)
{
    run([this, path, recipients, text]() {
        QString error;
        if (!ensureLoaded(&error))
            return failure(error);
        if (recipients.isEmpty())
            return failure(tr("No .gpg-id found for this entry"));

        rnp_input_t input = nullptr;
        QByteArray data = text.toUtf8();
        rnp_result_t result = rnp_input_from_memory(&input, reinterpret_cast<const uint8_t *>(data.constData()),
                                                    size_t(data.size()), false);
        AtomicOutput output(path);
        rnp_op_encrypt_t op = nullptr;
        if (result == RNP_SUCCESS)
            result = rnp_op_encrypt_create(&op, m_ffi, input, output.handle());

        QStringList missing;
        for (const QString &recipient : recipients) {
            if (result != RNP_SUCCESS)
                break;
            const QString fpr = findKeyFingerprint(recipient);
            rnp_key_handle_t key = nullptr;
            if (fpr.isEmpty()
                || rnp_locate_key(m_ffi, "fingerprint", fpr.toLatin1().constData(), &key) != RNP_SUCCESS
                || !key || rnp_op_encrypt_add_recipient(op, key) != RNP_SUCCESS)
                missing << recipient;
            rnp_key_handle_destroy(key);
        }

        QVariantMap map;
        if (result == RNP_SUCCESS && missing.isEmpty()) {
            rnp_op_encrypt_set_armor(op, false);
            result = rnp_op_encrypt_execute(op);
        }
        if (!missing.isEmpty()) {
            map = failure(tr("No usable public key for %1").arg(missing.join(QStringLiteral(", "))));
            map.insert(QStringLiteral("missing"), missing);
        } else if (result != RNP_SUCCESS) {
            map = failure(rnpError(result));
        } else if (!output.commit()) {
            map = failure(tr("Could not write %1").arg(path));
        } else {
            map = success();
        }
        rnp_op_encrypt_destroy(op);
        rnp_input_destroy(input);
        data.fill('\0');
        return map;
    }, callback);
}

void Pgp::listKeys(QJSValue callback)
{
    run([this]() {
        QString error;
        if (!ensureLoaded(&error))
            return failure(error);

        QVariantList keys;
        rnp_identifier_iterator_t it = nullptr;
        if (rnp_identifier_iterator_create(m_ffi, &it, "fingerprint") == RNP_SUCCESS) {
            const char *fpr = nullptr;
            while (rnp_identifier_iterator_next(it, &fpr) == RNP_SUCCESS && fpr) {
                rnp_key_handle_t key = nullptr;
                bool primary = false;
                if (rnp_locate_key(m_ffi, "fingerprint", fpr, &key) != RNP_SUCCESS || !key
                    || rnp_key_is_primary(key, &primary) != RNP_SUCCESS || !primary) {
                    rnp_key_handle_destroy(key);
                    continue;
                }
                QVariantMap entry;
                char *str = nullptr;
                entry.insert(QStringLiteral("fingerprint"), QString::fromLatin1(fpr));
                if (rnp_key_get_keyid(key, &str) == RNP_SUCCESS)
                    entry.insert(QStringLiteral("keyId"), takeString(str));
                if (rnp_key_get_alg(key, &str) == RNP_SUCCESS)
                    entry.insert(QStringLiteral("algorithm"), takeString(str));
                if (rnp_key_get_primary_uid(key, &str) == RNP_SUCCESS)
                    entry.insert(QStringLiteral("uid"), takeString(str));
                QStringList uids;
                size_t count = 0;
                rnp_key_get_uid_count(key, &count);
                for (size_t i = 0; i < count; ++i) {
                    if (rnp_key_get_uid_at(key, i, &str) == RNP_SUCCESS)
                        uids << takeString(str);
                }
                entry.insert(QStringLiteral("uids"), uids);
                bool secret = false;
                rnp_key_have_secret(key, &secret);
                entry.insert(QStringLiteral("secret"), secret);
                bool valid = false;
                rnp_key_is_valid(key, &valid);
                entry.insert(QStringLiteral("usable"), valid);
                keys << entry;
                rnp_key_handle_destroy(key);
            }
            rnp_identifier_iterator_destroy(it);
        }
        QVariantMap map = success();
        map.insert(QStringLiteral("keys"), keys);
        return map;
    }, jsCallback(callback));
}

void Pgp::importKeys(const QString &pathOrArmor, const QString &passphrase, QJSValue callback)
{
    run([this, pathOrArmor, passphrase]() {
        QString error;
        if (!ensureLoaded(&error))
            return failure(error);

        QByteArray data;
        if (!pathOrArmor.contains(QLatin1Char('\n')) && QFileInfo(pathOrArmor).isFile()) {
            QFile file(pathOrArmor);
            if (!file.open(QIODevice::ReadOnly))
                return failure(tr("Could not read %1").arg(pathOrArmor));
            data = file.readAll();
        } else {
            data = pathOrArmor.toUtf8();
        }

        const auto import = [this](const QByteArray &keys, char **results) {
            rnp_input_t input = nullptr;
            rnp_result_t result = rnp_input_from_memory(&input, reinterpret_cast<const uint8_t *>(keys.constData()),
                                                        size_t(keys.size()), false);
            if (result == RNP_SUCCESS)
                result = rnp_import_keys(m_ffi, input,
                                         RNP_LOAD_SAVE_PUBLIC_KEYS | RNP_LOAD_SAVE_SECRET_KEYS
                                             | RNP_LOAD_SAVE_PERMISSIVE,
                                         results);
            rnp_input_destroy(input);
            return result;
        };

        char *results = nullptr;
        rnp_result_t result = import(data, &results);
        if (result != RNP_SUCCESS) {
            // Not keys, but maybe keys encrypted with a passphrase
            m_providerAsked = false;
            m_importPassphrase = passphrase.toUtf8();
            rnp_input_t input = nullptr;
            rnp_output_t output = nullptr;
            rnp_result_t decrypted = rnp_input_from_memory(&input, reinterpret_cast<const uint8_t *>(data.constData()),
                                                           size_t(data.size()), false);
            if (decrypted == RNP_SUCCESS)
                decrypted = rnp_output_to_memory(&output, 0);
            if (decrypted == RNP_SUCCESS)
                decrypted = rnp_decrypt(m_ffi, input, output);
            m_importPassphrase.fill('\0');
            m_importPassphrase.clear();

            if (decrypted == RNP_SUCCESS) {
                uint8_t *buf = nullptr;
                size_t len = 0;
                rnp_output_memory_get_buf(output, &buf, &len, false);
                QByteArray keys(reinterpret_cast<const char *>(buf), int(len));
                rnp_buffer_clear(buf, len);
                result = import(keys, &results);
                keys.fill('\0');
            }
            rnp_output_destroy(output);
            rnp_input_destroy(input);

            if (decrypted != RNP_SUCCESS && m_providerAsked) {
                QVariantMap map = failure(passphrase.isEmpty() ? QString() : tr("Wrong passphrase"));
                map.insert(QStringLiteral("needPassphrase"), true);
                data.fill('\0');
                return map;
            }
            if (decrypted != RNP_SUCCESS) {
                data.fill('\0');
                return failure(tr("No OpenPGP keys found"));
            }
        }
        data.fill('\0');
        if (result != RNP_SUCCESS)
            return failure(tr("No OpenPGP keys found"));

        const QJsonArray imported = QJsonDocument::fromJson(takeString(results).toUtf8())
                                        .object().value(QStringLiteral("keys")).toArray();
        // Subkeys are listed too, count only the keys people think of
        int primaryCount = 0;
        for (const QJsonValue &entry : imported) {
            const QByteArray fpr = entry.toObject().value(QStringLiteral("fingerprint")).toString().toLatin1();
            rnp_key_handle_t key = nullptr;
            bool primary = false;
            if (rnp_locate_key(m_ffi, "fingerprint", fpr.constData(), &key) == RNP_SUCCESS && key
                && rnp_key_is_primary(key, &primary) == RNP_SUCCESS && primary)
                ++primaryCount;
            rnp_key_handle_destroy(key);
        }
        if (!saveKeys(&error))
            return failure(error);
        QVariantMap map = success();
        map.insert(QStringLiteral("imported"), primaryCount);
        return map;
    }, [this, callback](const QVariantMap &result) {
        if (result.value(QStringLiteral("ok")).toBool())
            emit keysChanged();
        JsCallback::call(callback, result);
    });
}

void Pgp::generateKey(const QString &userId, const QString &passphrase, QJSValue callback)
{
    run([this, userId, passphrase]() {
        QString error;
        if (!ensureLoaded(&error))
            return failure(error);
        QByteArray pass = passphrase.toUtf8();
        rnp_key_handle_t key = nullptr;
        rnp_result_t result = rnp_generate_key_25519(m_ffi, userId.toUtf8().constData(),
                                                     pass.isEmpty() ? nullptr : pass.constData(), &key);
        pass.fill('\0');
        if (result != RNP_SUCCESS)
            return failure(rnpError(result));
        QVariantMap map = success();
        char *fpr = nullptr;
        if (rnp_key_get_fprint(key, &fpr) == RNP_SUCCESS)
            map.insert(QStringLiteral("fingerprint"), takeString(fpr));
        rnp_key_handle_destroy(key);
        if (!saveKeys(&error))
            return failure(error);
        return map;
    }, [this, callback](const QVariantMap &result) {
        emit keysChanged();
        JsCallback::call(callback, result);
    });
}

void Pgp::exportPublicKey(const QString &fingerprint, QJSValue callback)
{
    run([this, fingerprint]() {
        QString error;
        if (!ensureLoaded(&error))
            return failure(error);
        rnp_key_handle_t key = nullptr;
        rnp_output_t output = nullptr;
        rnp_result_t result = rnp_locate_key(m_ffi, "fingerprint", fingerprint.toLatin1().constData(), &key);
        if (result == RNP_SUCCESS && !key)
            result = RNP_ERROR_KEY_NOT_FOUND;
        if (result == RNP_SUCCESS)
            result = rnp_output_to_memory(&output, 0);
        if (result == RNP_SUCCESS)
            result = rnp_key_export(key, output, RNP_KEY_EXPORT_ARMORED | RNP_KEY_EXPORT_PUBLIC
                                                     | RNP_KEY_EXPORT_SUBKEYS);
        QVariantMap map;
        if (result == RNP_SUCCESS) {
            uint8_t *buf = nullptr;
            size_t len = 0;
            rnp_output_memory_get_buf(output, &buf, &len, false);
            map = success();
            map.insert(QStringLiteral("armor"), QString::fromLatin1(reinterpret_cast<const char *>(buf), int(len)));
        } else {
            map = failure(rnpError(result));
        }
        rnp_output_destroy(output);
        rnp_key_handle_destroy(key);
        return map;
    }, jsCallback(callback));
}

void Pgp::removeKey(const QString &fingerprint, QJSValue callback)
{
    run([this, fingerprint]() {
        QString error;
        if (!ensureLoaded(&error))
            return failure(error);
        rnp_key_handle_t key = nullptr;
        rnp_result_t result = rnp_locate_key(m_ffi, "fingerprint", fingerprint.toLatin1().constData(), &key);
        if (result == RNP_SUCCESS && !key)
            result = RNP_ERROR_KEY_NOT_FOUND;
        if (result == RNP_SUCCESS)
            result = rnp_key_remove(key, RNP_KEY_REMOVE_PUBLIC | RNP_KEY_REMOVE_SECRET | RNP_KEY_REMOVE_SUBKEYS);
        rnp_key_handle_destroy(key);
        if (result != RNP_SUCCESS)
            return failure(rnpError(result));
        if (!saveKeys(&error))
            return failure(error);
        return success();
    }, [this, callback](const QVariantMap &result) {
        emit keysChanged();
        JsCallback::call(callback, result);
    });
}
