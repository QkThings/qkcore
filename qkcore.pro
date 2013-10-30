#-------------------------------------------------
#
# Project created by QtCreator 2013-07-24T20:12:31
#
#-------------------------------------------------

QT       -= gui

TARGET = qkcore
TEMPLATE = lib
INCLUDEPATH += private
INCLUDEPATH += comm

DEFINES += QKLIB_LIBRARY

SOURCES += \
    qkcore.cpp \
    comm/qk_comm.cpp

HEADERS +=\
    qklib_global.h \
    qk_defs.h \
    private/qk_utils.h \
    qk_comm.h \
    qkcore.h

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



