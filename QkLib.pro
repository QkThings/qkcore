#-------------------------------------------------
#
# Project created by QtCreator 2013-07-24T20:12:31
#
#-------------------------------------------------

QT       -= gui

TARGET = QkLib
TEMPLATE = lib

DEFINES += QKLIB_LIBRARY

SOURCES += qk.cpp

HEADERS += qk.h\
        qklib_global.h \
    qk_defs.h

unix:!symbian {
    maemo5 {
        target.path = /opt/usr/lib
    } else {
        target.path = /usr/lib
    }
    INSTALLS += target
}

CONFIG(debug, debug|release) {
    DESTDIR = build/debug
} else {
    DESTDIR = build/release
}

OBJECTS_DIR = build/tmp/obj
MOC_DIR = build/tmp/moc
RCC_DIR = build/tmp/rcc
UI_DIR = build/tmp/ui
