CONFIG += console
macx {
CONFIG-=app_bundle
}
TARGET = assToTtxt
QT += core xml
SOURCES += main.cpp
