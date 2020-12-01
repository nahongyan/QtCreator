DEFINES += EXTENSIONSYSTEM_LIBRARY
include(../../qtcreatorlibrary.pri)

unix:LIBS += $$QMAKE_LIBS_DYNLOAD

!isEmpty(vcproj) {
    DEFINES += IDE_TEST_DIR=\"$$IDE_SOURCE_TREE\"
} else {
    DEFINES += IDE_TEST_DIR=\\\"$$IDE_SOURCE_TREE\\\"
}

#Qt Creator 在源代码中使用了大量 D-Pointer 设计，被称为 D 指针，这里有必要先进行一些补充说明，以便更好理解。
#D 指针是 C++ 特有的一种“设计模式”，并不适用于 Java 等其它语言（这是与另外一些设计模式区别的地方）。
#D 指针主要为了解决二进制兼容的问题。二进制兼容与源代码兼容
#一个动态链接到较早版本的库的程序，在不经过重新编译的情况下能够继续运行在新版本的库，这样的库即“二进制兼容”。
#一个程序不需要经过额外的修改，只需要重新编译就能够在新版本的库下继续运行，这样的库被称为“源代码兼容”。
#二进制兼容对于库尤其重要，因为你不能指望每次升级了库之后，要求所有使用了这个库的应用程序都要重新编译。
#就 Qt 而言，假如一个程序使用 Qt 5.5 编译，当 Qt 5.6 发布之后，如果 Qt 的升级能够保持二进制兼容，
#那么这个程序应该可以直接能够使用 Qt 5.6 正常运行；否则，就需要这个程序重新编译，才能够运行在 Qt 5.6 平台。
#要使一个库能够实现二进制兼容，就要求每一个结构以及每一个对象的数据模型保持不变。
#所谓“数据模型保持不变”，就是不能在类中增加、删除数据成员。这是 C++ 编译器要求的，其根本是保持数据成员在数据模型中的位移保持不变。
HEADERS += pluginerrorview.h \
    plugindetailsview.h \
    invoker.h \
    iplugin.h \
    iplugin_p.h \
    extensionsystem_global.h \
    pluginmanager.h \
    pluginmanager_p.h \
    pluginspec.h \
    pluginspec_p.h \
    pluginview.h \
    optionsparser.h \
    pluginerroroverview.h
SOURCES += pluginerrorview.cpp \
    plugindetailsview.cpp \
    invoker.cpp \
    iplugin.cpp \
    pluginmanager.cpp \
    pluginspec.cpp \
    pluginview.cpp \
    optionsparser.cpp \
    pluginerroroverview.cpp
FORMS += \
    pluginerrorview.ui \
    plugindetailsview.ui \
    pluginerroroverview.ui
