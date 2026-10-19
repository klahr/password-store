#include "passwordentry.h"

#include <QDateTime>
#include <QMessageAuthenticationCode>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <QVariantList>

namespace {

const char *const PasswordPrefixes[] = { "password:", "secret:", "pass:" };
const char *const UsernamePrefixes[] = { "login:", "username:", "user:", "account:", "email:", "mail:",
                                         "name:", "handle:", "id:", "identity:" };
const char SteamAlphabet[] = "23456789BCDFGHJKMNPQRTVWXY";

QStringList splitLines(const QString &content)
{
    QStringList lines = content.split(QLatin1Char('\n'));
    for (QString &line : lines) {
        if (line.endsWith(QLatin1Char('\r')))
            line.chop(1);
    }
    while (!lines.isEmpty() && lines.last().isEmpty())
        lines.removeLast();
    return lines;
}

QString leftTrimmed(const QString &str)
{
    int i = 0;
    while (i < str.length() && str.at(i).isSpace())
        ++i;
    return str.mid(i);
}

bool isTotpLine(const QString &line)
{
    return line.startsWith(QLatin1String("otpauth://totp"))
           || line.startsWith(QLatin1String("totp:"), Qt::CaseInsensitive);
}

// Length of the matching prefix including the whitespace after it, or 0
template <size_t N>
int prefixLength(const QString &line, const char *const (&prefixes)[N])
{
    for (const char *prefix : prefixes) {
        const QLatin1String p(prefix);
        if (line.startsWith(p, Qt::CaseInsensitive))
            return line.length() - leftTrimmed(line.mid(p.size())).length();
    }
    return 0;
}

// Index of the password line and the length of its prefix. Like pass, that
// is the first line, unless it is a TOTP line: then a "password:" style
// line, or -1 for none.
int findPassword(const QStringList &lines, int *prefixLen)
{
    *prefixLen = 0;
    if (lines.isEmpty())
        return 0;
    if (!isTotpLine(lines.first()))
        return 0;
    for (int i = 1; i < lines.size(); ++i) {
        const int len = prefixLength(lines.at(i), PasswordPrefixes);
        if (len > 0) {
            *prefixLen = len;
            return i;
        }
    }
    return -1;
}

QVariantMap parseTotp(const QString &line)
{
    QVariantMap totp;
    QString secret;
    QString algorithm = QStringLiteral("SHA1");
    QString digits = QStringLiteral("6");
    int period = 30;
    QString issuer;

    if (line.startsWith(QLatin1String("otpauth://"))) {
        const QUrl url(line);
        const QUrlQuery query(url);
        secret = query.queryItemValue(QStringLiteral("secret"), QUrl::FullyDecoded);
        if (query.hasQueryItem(QStringLiteral("algorithm")))
            algorithm = query.queryItemValue(QStringLiteral("algorithm")).toUpper();
        if (query.hasQueryItem(QStringLiteral("digits")))
            digits = query.queryItemValue(QStringLiteral("digits"));
        if (query.hasQueryItem(QStringLiteral("period")))
            period = query.queryItemValue(QStringLiteral("period")).toInt();
        issuer = query.queryItemValue(QStringLiteral("issuer"), QUrl::FullyDecoded);
        if (issuer.isEmpty()) {
            // The label is "issuer:account" or just "account"
            const QString label = url.path(QUrl::FullyDecoded).mid(1);
            if (label.contains(QLatin1Char(':')))
                issuer = label.section(QLatin1Char(':'), 0, 0);
        }
    } else {
        secret = leftTrimmed(line.mid(line.indexOf(QLatin1Char(':')) + 1));
    }

    bool ok = false;
    PasswordEntry::base32Decode(secret, &ok);
    const bool steam = digits == QLatin1String("s") || issuer == QLatin1String("Steam");
    const int digitCount = steam ? 5 : digits.toInt();
    if (!ok || period <= 0 || (!steam && (digitCount < 6 || digitCount > 10))
        || !(algorithm == QLatin1String("SHA1") || algorithm == QLatin1String("SHA256")
             || algorithm == QLatin1String("SHA512")))
        return totp;

    totp.insert(QStringLiteral("secret"), secret);
    totp.insert(QStringLiteral("algorithm"), algorithm);
    totp.insert(QStringLiteral("digits"), digitCount);
    totp.insert(QStringLiteral("period"), period);
    totp.insert(QStringLiteral("steam"), steam);
    totp.insert(QStringLiteral("issuer"), issuer);
    return totp;
}

}

PasswordEntry::PasswordEntry(QObject *parent)
    : QObject(parent)
{
}

QVariantMap PasswordEntry::parse(const QString &content)
{
    QStringList lines = splitLines(content);
    QVariantMap result;

    int prefixLen = 0;
    const int passwordLine = findPassword(lines, &prefixLen);
    result.insert(QStringLiteral("hasPassword"), passwordLine >= 0 && !lines.isEmpty());
    if (passwordLine >= 0 && !lines.isEmpty()) {
        result.insert(QStringLiteral("password"), lines.at(passwordLine).mid(prefixLen));
        lines.removeAt(passwordLine);
    }

    QVariantMap totp;
    QString username;
    QVariantList fields;
    QStringList extra;
    for (const QString &line : lines) {
        if (isTotpLine(line)) {
            if (totp.isEmpty())
                totp = parseTotp(line);
            continue;
        }
        if (username.isEmpty()) {
            const int len = prefixLength(line, UsernamePrefixes);
            if (len > 0 && len < line.length()) {
                username = line.mid(len);
                continue;
            }
        }
        const int colon = line.indexOf(QLatin1Char(':'));
        const QString key = line.left(colon).trimmed();
        const QString value = leftTrimmed(line.mid(colon + 1));
        // A bare URL is content, not a field called "https"
        if (colon < 0 || key.isEmpty() || value.isEmpty() || value.startsWith(QLatin1String("//"))) {
            extra << line;
        } else {
            QVariantMap field;
            field.insert(QStringLiteral("key"), key);
            field.insert(QStringLiteral("value"), value);
            fields << field;
        }
    }

    result.insert(QStringLiteral("username"), username);
    result.insert(QStringLiteral("fields"), fields);
    result.insert(QStringLiteral("extraContent"), extra.join(QLatin1Char('\n')).trimmed());
    result.insert(QStringLiteral("hasTotp"), !totp.isEmpty());
    result.insert(QStringLiteral("totp"), totp);
    return result;
}

QByteArray PasswordEntry::base32Decode(const QString &input, bool *ok)
{
    QByteArray result;
    quint32 buffer = 0;
    int bits = 0;
    *ok = false;
    for (const QChar ch : input) {
        const char c = ch.toUpper().toLatin1();
        int value;
        if (c >= 'A' && c <= 'Z')
            value = c - 'A';
        else if (c >= '2' && c <= '7')
            value = c - '2' + 26;
        else if (c == ' ' || c == '-' || c == '=')
            continue;
        else
            return QByteArray();
        buffer = (buffer << 5) | quint32(value);
        bits += 5;
        if (bits >= 8) {
            bits -= 8;
            result.append(char((buffer >> bits) & 0xff));
        }
    }
    *ok = !result.isEmpty();
    return result;
}

QString PasswordEntry::totpCode(const QVariantMap &totp, qint64 epochSeconds)
{
    bool ok = false;
    const QByteArray key = base32Decode(totp.value(QStringLiteral("secret")).toString(), &ok);
    const int period = totp.value(QStringLiteral("period")).toInt();
    const int digits = totp.value(QStringLiteral("digits")).toInt();
    if (!ok || period <= 0 || digits <= 0)
        return QString();

    const QString algorithm = totp.value(QStringLiteral("algorithm")).toString();
    QCryptographicHash::Algorithm method = QCryptographicHash::Sha1;
    if (algorithm == QLatin1String("SHA256"))
        method = QCryptographicHash::Sha256;
    else if (algorithm == QLatin1String("SHA512"))
        method = QCryptographicHash::Sha512;

    quint64 counter = quint64(epochSeconds / period);
    QByteArray message(8, '\0');
    for (int i = 7; i >= 0; --i) {
        message[i] = char(counter & 0xff);
        counter >>= 8;
    }
    const QByteArray hash = QMessageAuthenticationCode::hash(message, key, method);
    const int offset = hash.at(hash.size() - 1) & 0x0f;
    quint32 binary = (quint32(hash.at(offset) & 0x7f) << 24)
                     | (quint32(quint8(hash.at(offset + 1))) << 16)
                     | (quint32(quint8(hash.at(offset + 2))) << 8)
                     | quint32(quint8(hash.at(offset + 3)));

    if (totp.value(QStringLiteral("steam")).toBool()) {
        QString code;
        for (int i = 0; i < digits; ++i) {
            code.append(QLatin1Char(SteamAlphabet[binary % 26]));
            binary /= 26;
        }
        return code;
    }

    qint64 modulo = 1;
    for (int i = 0; i < digits; ++i)
        modulo *= 10;
    return QStringLiteral("%1").arg(qint64(binary) % modulo, digits, 10, QLatin1Char('0'));
}

QString PasswordEntry::totpCode(const QVariantMap &totp)
{
    return totpCode(totp, QDateTime::currentMSecsSinceEpoch() / 1000);
}

int PasswordEntry::totpRemaining(const QVariantMap &totp, qint64 epochSeconds)
{
    const int period = totp.value(QStringLiteral("period")).toInt();
    if (period <= 0)
        return 0;
    return period - int(epochSeconds % period);
}

int PasswordEntry::totpRemaining(const QVariantMap &totp)
{
    return totpRemaining(totp, QDateTime::currentMSecsSinceEpoch() / 1000);
}

QVariantMap PasswordEntry::splitForEdit(const QString &content)
{
    QStringList lines = splitLines(content);
    int prefixLen = 0;
    const int line = findPassword(lines, &prefixLen);
    QVariantMap result;
    result.insert(QStringLiteral("line"), line);
    if (line >= 0 && line < lines.size()) {
        result.insert(QStringLiteral("prefix"), lines.at(line).left(prefixLen));
        result.insert(QStringLiteral("password"), lines.at(line).mid(prefixLen));
        lines.removeAt(line);
    } else {
        result.insert(QStringLiteral("prefix"), QString());
        result.insert(QStringLiteral("password"), QString());
    }
    result.insert(QStringLiteral("rest"), lines.join(QLatin1Char('\n')));
    return result;
}

QString PasswordEntry::joinForEdit(const QString &password, const QString &rest, int line,
                                   const QString &prefix)
{
    QStringList lines = splitLines(rest);
    if (line >= 0 || !password.isEmpty())
        lines.insert(qBound(0, line, lines.size()), prefix + password);
    return lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}
