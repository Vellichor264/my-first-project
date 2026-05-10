QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    gamewidget.cpp \
    startmenu.cpp

HEADERS += \
    gamewidget.h \
    startmenu.h \
    startmenu.h

FORMS += \
    gamewidget.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
# 定义复制命令（Windows 用 copy，Linux/Mac 用 cp）
win32 {
    COPYPATH = $$PWD/levels
    DESTPATH = $$OUT_PWD
    copydata.commands = xcopy /Y /I /E $$shell_path($$COPYPATH) $$shell_path($$DESTPATH) > nul
} else {
    copydata.commands = cp -r $$PWD/levels $$OUT_PWD
}

# 让构建过程依赖 copydata
first.depends = $(first) copydata
export(first.depends)
export(copydata.commands)

# 添加自定义目标
QMAKE_EXTRA_TARGETS += first copydata
