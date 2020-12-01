#前面已经介绍过$$replace()函数；第一个参数_PRO_FILE_PWD_同样说过。
#这里值得说明的是后面两个参数。第二个参数([^/]+$)是一个正则表达式。qmake 的正则表达式规则同QRegExp类似，可以参数QRegExp的文档。
#我们从内向外读。[^/]+$匹配字符串的最后一个 / 字符直到最后结尾的子串；()则是捕获匹配的子串，后面则可以使用\1替换
#例如，_PRO_FILE_PWD_的值是E:/Sources/qt-creator/src/libs/aggregation，匹配[^/]+$的部分是aggregation，
#使用()则将该字符串捕获到\1，最后的\\1/\\1_dependencies.pri部分最终结果是aggregation/aggregation_dependencies.pri。
#$$replace()函数替换之后的结果是E:/Sources/qt-creator/src/libs/aggregation/aggregation_dependencies.pri
include($$replace(_PRO_FILE_PWD_, ([^/]+$), \\1/\\1_dependencies.pri))

#QTC_LIB_NAME正是在 aggregation_dependencies.pri 中定义的；可以通过打开这个文件找到具体的值。
#此时TARGET值被设置为Aggregation。因此，我们的类库名字就是 Aggregation
TARGET = $$QTC_LIB_NAME

include(../qtcreator.pri)

# 默认情况下使用库的预编译头
isEmpty(PRECOMPILED_HEADER):PRECOMPILED_HEADER = $$PWD/shared/qtcreator_pch.h

win32 {
    DLLDESTDIR = $$IDE_APP_PATH
}

DESTDIR = $$IDE_LIBRARY_PATH

osx {
    QMAKE_LFLAGS_SONAME = -Wl,-install_name,@rpath/
    QMAKE_LFLAGS += -compatibility_version $$QTCREATOR_COMPAT_VERSION
}

RPATH_BASE = $$IDE_LIBRARY_PATH
include(rpath.pri)

TARGET = $$qtLibraryTargetName($$TARGET)

TEMPLATE = lib
CONFIG += shared dll

contains(QT_CONFIG, reduce_exports):CONFIG += hide_symbols

win32 {
    dlltarget.path = $$INSTALL_BIN_PATH
    INSTALLS += dlltarget
} else {
    target.path = $$INSTALL_LIBRARY_PATH
    INSTALLS += target
}
