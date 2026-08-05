QT       += core
QT       -= gui

TARGET    = kvserializer_demo
TEMPLATE  = app
CONFIG   += console
CONFIG   -= app_bundle

# QT 4.8.5 / RK3568
# 交叉编译示例（按实际 SDK 调整）：
#   qmake kvserializer.pro -spec devices/linux-buildroot-g++
#   make

SOURCES += \
    src/main.cpp \
    src/KvSerializer.cpp

HEADERS += \
    src/KvSerializer.h

# 保证源文件按 UTF-8 解析（中文测试数据）
CODECFORSRC = UTF-8
CODECFORTR  = UTF-8
