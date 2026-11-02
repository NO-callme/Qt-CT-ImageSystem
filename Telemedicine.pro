#-------------------------------------------------
#
# 远程医疗 CT 诊断系统：Qt 6 + MySQL(libmysql) + OpenCV(MinGW)
#
#-------------------------------------------------

QT       += core gui widgets

TARGET = Telemedicine
TEMPLATE = app

CONFIG += c++17
CONFIG -= debug_and_release   # 只构建一种配置，避免 debug/release 子目录
DESTDIR = $$OUT_PWD           # exe 直接输出到构建目录根，方便定位运行依赖
DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += \
        main.cpp \
        mainwindow.cpp \
        db.cpp

HEADERS += \
        mainwindow.h \
        db.h

FORMS += \
        mainwindow.ui

# OpenCV（MSYS2 MINGW64 预编译包），路径按实际安装位置调整
# 注意：按完整路径链接 .dll.a，避免 -L 把 MSYS2 的运行库带进搜索路径、与 Qt 自带 MinGW 冲突
OPENCV_DIR = C:/msys64/mingw64
INCLUDEPATH += $$OPENCV_DIR/include/opencv5
LIBS += $$OPENCV_DIR/lib/libopencv_core.dll.a \
        $$OPENCV_DIR/lib/libopencv_imgproc.dll.a \
        $$OPENCV_DIR/lib/libopencv_imgcodecs.dll.a

# MySQL C API（libmysql）。libmysql.a 由 gendef + dlltool 从
# "C:\Program Files\MySQL\MySQL Server 8.0\lib\libmysql.dll" 生成，见 3rdparty/libmysql
MYSQL_DIR = "C:/Program Files/MySQL/MySQL Server 8.0"
INCLUDEPATH += "$$MYSQL_DIR/include"
LIBS += $$PWD/3rdparty/libmysql/libmysql.a

# 构建后自动把数据文件 / dbconfig.ini / OpenCV 与 MySQL 运行时 DLL 复制到 exe 目录
QMAKE_POST_LINK += python $$PWD/deploy_runtime.py $$DESTDIR
