TEMPLATE  = subdirs
CONFIG   += ordered

SUBDIRS += \
    shared \
    libs \
    app \
    plugins \
#    tools \
    share/qtcreator/data.pro \
    share/3rdparty/data.pro

# 委托 deployqt 目标
deployqt.CONFIG += recursive
deployqt.recurse = shared libs app plugins #tools
QMAKE_EXTRA_TARGETS += deployqt
