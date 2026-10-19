#ifndef PASSWORDENTRY_H
#define PASSWORDENTRY_H

#include <QObject>
#include <QString>
#include <QVariantMap>

// Reads the contents of a pass entry: the first line is the password, as pass
// sees it, unless it is a TOTP line, then a "password:" style line is.
// Like Android Password Store, "user:" style lines give the username,
// "otpauth://totp" or "totp:" lines a TOTP secret and "key: value" lines
// become fields.
class PasswordEntry : public QObject
{
    Q_OBJECT

public:
    explicit PasswordEntry(QObject *parent = nullptr);

    // Result: hasPassword, password, username, fields [{key, value}],
    // extraContent, hasTotp, totp {secret, algorithm, digits, period, steam, issuer}
    Q_INVOKABLE static QVariantMap parse(const QString &content);
    // An empty string when the TOTP parameters can't give a code
    Q_INVOKABLE static QString totpCode(const QVariantMap &totp, qint64 epochSeconds);
    Q_INVOKABLE static QString totpCode(const QVariantMap &totp);
    // Seconds left until the code at epochSeconds changes
    Q_INVOKABLE static int totpRemaining(const QVariantMap &totp, qint64 epochSeconds);
    Q_INVOKABLE static int totpRemaining(const QVariantMap &totp);
    // Splits content into the password and everything else for editing.
    // Result: password, rest, line, prefix ("" when the password is the first line)
    Q_INVOKABLE static QVariantMap splitForEdit(const QString &content);
    // Puts a password back where splitForEdit found it
    Q_INVOKABLE static QString joinForEdit(const QString &password, const QString &rest, int line,
                                           const QString &prefix);

    static QByteArray base32Decode(const QString &input, bool *ok);
};

#endif // PASSWORDENTRY_H
