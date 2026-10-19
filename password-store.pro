# NOTICE:
#
# Application name defined in TARGET has a corresponding QML filename.
# If name defined in TARGET is changed, the following needs to be done
# to match new name:
#   - corresponding QML filename must be changed
#   - desktop icon filename must be changed
#   - desktop filename must be changed
#   - icon definition filename in desktop file must be changed
#   - translation filenames have to be changed

# The name of your application
TARGET = password-store

CONFIG += sailfishapp c++14
QT += concurrent
PKGCONFIG += openssl json-c zlib

VERSION = 0.3.2
DEFINES += APP_VERSION=\\\"$$VERSION\\\"

SOURCES += src/password-store.cpp \
    src/clipboard.cpp \
    src/git.cpp \
    src/jscallback.cpp \
    src/passstore.cpp \
    src/passwordentry.cpp \
    src/passwordgenerator.cpp \
    src/pgp.cpp \
    src/qrscanner.cpp

HEADERS += src/clipboard.h \
    src/git.h \
    src/jscallback.h \
    src/passstore.h \
    src/passwordentry.h \
    src/passwordgenerator.h \
    src/pgp.h \
    src/qrscanner.h

# rnp (BSD-2-Clause) does the OpenPGP work. The device's gnupg2 is 2.0 and
# can't handle the ed25519/cv25519 keys or AEAD messages current GnuPG makes,
# so rnp is built from 3rdparty/rnp against the system OpenSSL 3 and linked
# statically.
RNP_SRC = $$PWD/3rdparty/rnp
RNP_BUILD = $$OUT_PWD/rnp-build

rnp.target = $$RNP_BUILD/src/lib/librnp.a
rnp.commands = cmake -S $$RNP_SRC -B $$RNP_BUILD \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=OFF -DENABLE_DOC=Off \
    -DCRYPTO_BACKEND=openssl && \
    cmake --build $$RNP_BUILD --target librnp --parallel
# These objects include the generated rnp_export.h
pgp_rnp.target = pgp.o
pgp_rnp.depends = $$rnp.target
QMAKE_EXTRA_TARGETS += rnp pgp_rnp
PRE_TARGETDEPS += $$rnp.target

INCLUDEPATH += $$RNP_SRC/include $$RNP_BUILD/src/lib
LIBS += $$RNP_BUILD/src/lib/librnp.a $$RNP_BUILD/src/libsexpp/libsexpp.a -lbz2

# quirc (ISC) reads QR codes, small enough to compile straight into the app
QUIRC_SRC = $$PWD/3rdparty/quirc/lib
INCLUDEPATH += $$QUIRC_SRC
SOURCES += $$QUIRC_SRC/decode.c $$QUIRC_SRC/identify.c $$QUIRC_SRC/quirc.c $$QUIRC_SRC/version_db.c
QMAKE_CFLAGS += -std=c99

DISTFILES += qml/password-store.qml \
    qml/cover/CoverPage.qml \
    qml/components/*.qml \
    qml/pages/*.qml \
    rpm/password-store.changes \
    rpm/password-store.spec \
    translations/*.ts \
    password-store.desktop \
    sailjail/password-store.profile \
    LICENSE \
    README.md \
    icons/password-store.png

# Licenses of the app and of what is bundled in it
licenses.files = LICENSE 3rdparty/rnp/LICENSE.md 3rdparty/rnp/LICENSE-OCB.md
licenses.path = /usr/share/$${TARGET}/licenses
quirc_license.files = 3rdparty/quirc/LICENSE
quirc_license.path = /usr/share/$${TARGET}/licenses/quirc
INSTALLS += licenses quirc_license

# The sandbox only has the app's own binary, this adds git and ssh
sailjail.files = sailjail/password-store.profile
sailjail.path = /etc/sailjail/permissions
INSTALLS += sailjail

SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172

# Keeps translations/password-store.ts up to date as the source for translators.
# Add translations/password-store-<lang>.ts files to TRANSLATIONS to ship them.
CONFIG += sailfishapp_i18n
