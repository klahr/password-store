#include "passwordgenerator.h"

#include <QStringList>

#include <openssl/rand.h>

#include <cstdlib>

namespace {

const QString Digits = QStringLiteral("0123456789");
const QString Uppercase = QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
const QString Lowercase = QStringLiteral("abcdefghijklmnopqrstuvwxyz");
const QString Symbols = QStringLiteral("!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~");
const QString Ambiguous = QStringLiteral("B8G6I1l0OQDS5Z2");

QString withoutAmbiguous(QString chars)
{
    for (const QChar c : Ambiguous)
        chars.remove(c);
    return chars;
}

bool containsAny(const QString &str, const QString &chars)
{
    for (const QChar c : str) {
        if (chars.contains(c))
            return true;
    }
    return false;
}

}

PasswordGenerator::PasswordGenerator(QObject *parent)
    : QObject(parent)
{
}

quint32 PasswordGenerator::randomBelow(quint32 bound)
{
    if (bound <= 1)
        return 0;
    // Rejection sampling keeps the result unbiased
    const quint32 limit = quint32(0xffffffffu) - (quint32(0xffffffffu) % bound);
    quint32 value;
    do {
        if (RAND_bytes(reinterpret_cast<unsigned char *>(&value), sizeof(value)) != 1)
            abort();
    } while (value >= limit);
    return value % bound;
}

QString PasswordGenerator::classic(int length, bool digits, bool uppercase, bool lowercase, bool symbols,
                                   bool avoidAmbiguous) const
{
    QStringList classes;
    if (digits)
        classes << Digits;
    if (uppercase)
        classes << Uppercase;
    if (lowercase)
        classes << Lowercase;
    if (symbols)
        classes << Symbols;
    if (avoidAmbiguous) {
        for (QString &chars : classes)
            chars = withoutAmbiguous(chars);
    }
    if (classes.isEmpty() || length < classes.size())
        return QString();

    const QString all = classes.join(QString());
    QString password;
    for (int attempt = 0; attempt < 1000; ++attempt) {
        password.clear();
        for (int i = 0; i < length; ++i)
            password.append(all.at(int(randomBelow(quint32(all.size())))));
        bool valid = true;
        for (const QString &chars : classes)
            valid = valid && containsAny(password, chars);
        if (valid)
            return password;
    }
    return password;
}
