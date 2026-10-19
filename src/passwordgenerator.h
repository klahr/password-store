#ifndef PASSWORDGENERATOR_H
#define PASSWORDGENERATOR_H

#include <QObject>
#include <QString>

// Random passwords from character classes, with the OpenSSL CSPRNG
class PasswordGenerator : public QObject
{
    Q_OBJECT

public:
    explicit PasswordGenerator(QObject *parent = nullptr);

    // An empty string when no character class is selected or length is too
    // short to hold one character of each selected class
    Q_INVOKABLE QString classic(int length, bool digits, bool uppercase, bool lowercase, bool symbols,
                                bool avoidAmbiguous) const;

    // Uniform in [0, bound)
    static quint32 randomBelow(quint32 bound);
};

#endif // PASSWORDGENERATOR_H
