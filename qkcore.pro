#-------------------------------------------------
#
# Project created by QtCreator 2013-07-24T20:12:31
#
#-------------------------------------------------

QT       -= gui

TARGET = qkcore
TEMPLATE = lib
INCLUDEPATH += ../utils

DEFINES += QT_NO_DEBUG_OUTPUT

DEFINES += QKLIB_LIBRARY

SOURCES += \
    qkcore.cpp \
    ../utils/qkutils.cpp \
    qkcomm.cpp \
    qkdevice.cpp \
    qkboard.cpp \
    qknode.cpp \
    qkprotocol.cpp \
    qkconnect.cpp \
    qkconnserial.cpp

HEADERS +=\
    qkcore.h \
    qkprotocol.h \
    ../utils/qkutils.h \
    qkcomm.h \
    qkdevice.h \
    qkboard.h \
    qknode.h \
    qkcore_lib.h \
    qkcore_constants.h \
    qkconnserial.h \
    qkconnect.h

unix:!symbian {
    maemo5 {
        target.path = /opt/usr/lib
    } else {
        target.path = /usr/lib
    }
    INSTALLS += target
}

CONFIG(debug, debug|release) {
    DESTDIR = debug
} else {
    DESTDIR = release
}

OBJECTS_DIR = build/obj
MOC_DIR = build/moc
RCC_DIR = build/rcc
UI_DIR = build/ui



