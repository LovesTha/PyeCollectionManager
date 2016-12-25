#-------------------------------------------------
#
# Project created by QtCreator 2016-04-05T19:52:43
#
#-------------------------------------------------

QT       += core gui network xml multimedia sql

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = PyeCollectionManager
TEMPLATE = app


SOURCES += main.cpp\
        pcmwindow.cpp \
    ../GLib/oracle.cpp

HEADERS  += pcmwindow.h \
    ../GLib/oracle.h

FORMS    += pcmwindow.ui
