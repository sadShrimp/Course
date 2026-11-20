QT       += core gui widgets sql
CONFIG   += c++17
TARGET    = HoldingAnalytics
TEMPLATE  = app

# Исходники в UTF-8 (русские строки в коде)
msvc: QMAKE_CXXFLAGS += /utf-8

SOURCES += \
    main.cpp \
    database.cpp \
    dataservice.cpp \
    analytics.cpp \
    crudtab.cpp \
    analysttabs.cpp \
    rolewindow.cpp \
    loginwindow.cpp

HEADERS += \
    database.h \
    dataservice.h \
    analytics.h \
    crudtab.h \
    analysttabs.h \
    rolewindow.h \
    loginwindow.h
