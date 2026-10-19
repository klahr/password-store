# Host unit tests: qmake tests/tests.pro && make && ./tst_core
# Needs gpg on the host for the interoperability tests.
TEMPLATE = app
TARGET = tst_core
QT += testlib qml concurrent
CONFIG += c++14 testcase link_pkgconfig
PKGCONFIG += openssl json-c zlib

SRC = $$PWD/../src
INCLUDEPATH += $$SRC
SOURCES += tst_core.cpp \
    $$SRC/git.cpp \
    $$SRC/jscallback.cpp \
    $$SRC/passstore.cpp \
    $$SRC/passwordentry.cpp \
    $$SRC/passwordgenerator.cpp \
    $$SRC/pgp.cpp \
    $$SRC/qrscanner.cpp
HEADERS += $$SRC/git.h \
    $$SRC/jscallback.h \
    $$SRC/passstore.h \
    $$SRC/passwordentry.h \
    $$SRC/passwordgenerator.h \
    $$SRC/pgp.h \
    $$SRC/qrscanner.h

QUIRC_SRC = $$PWD/../3rdparty/quirc/lib
INCLUDEPATH += $$QUIRC_SRC
SOURCES += $$QUIRC_SRC/decode.c $$QUIRC_SRC/identify.c $$QUIRC_SRC/quirc.c $$QUIRC_SRC/version_db.c

RNP_SRC = $$PWD/../3rdparty/rnp
RNP_BUILD = $$OUT_PWD/rnp-build
rnp.target = $$RNP_BUILD/src/lib/librnp.a
rnp.commands = cmake -S $$RNP_SRC -B $$RNP_BUILD \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=OFF -DENABLE_DOC=Off \
    -DCRYPTO_BACKEND=openssl && \
    cmake --build $$RNP_BUILD --target librnp --parallel
pgp_rnp.target = pgp.o
pgp_rnp.depends = $$rnp.target
QMAKE_EXTRA_TARGETS += rnp pgp_rnp
PRE_TARGETDEPS += $$rnp.target
INCLUDEPATH += $$RNP_SRC/include $$RNP_BUILD/src/lib
LIBS += $$RNP_BUILD/src/lib/librnp.a $$RNP_BUILD/src/libsexpp/libsexpp.a -lbz2
