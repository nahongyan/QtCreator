#include函数，和C、C++中#include类似：
#将qtcreator.pri的文件内容添加到本pro文件中，注意文件后缀为“pri”
include(qtcreator.pri)

#qt版本检查 ,minQtVersion函数是在qtcreator.pri中定义的
#$$()或$${}：获取环境变量；
#$$：展开变量；
#$()：将会把$()传入Makefile中，在Makefile中使用$()
#$$[]：从qmake中获取属性，qmake中内置了很多属性
#如下：
#message(Qt version: $$[QT_VERSION])
#message(Qt is installed in $$[QT_INSTALL_PREFIX])
#message(Qt resources can be found in the following locations:)
#message(Documentation: $$[QT_INSTALL_DOCS])
#message(Header files: $$[QT_INSTALL_HEADERS])
#message(Libraries: $$[QT_INSTALL_LIBS])
#message(Binary files (executables): $$[QT_INSTALL_BINS])
#message(Plugins: $$[QT_INSTALL_PLUGINS])
#message(Data files: $$[QT_INSTALL_DATA])
#message(Translation files: $$[QT_INSTALL_TRANSLATIONS])
#message(Settings: $$[QT_INSTALL_CONFIGURATION])
#message(Examples: $$[QT_INSTALL_EXAMPLES])

!minQtVersion(5, 14, 0) {
    message("Cannot build $$IDE_DISPLAY_NAME with Qt version $${QT_VERSION}.")
    error("Use at least Qt 5.14.0.")
}

#生成doxygen文档的配置文件
#include(doc/doc.pri)

#TEMPLATE即代码模板，将告诉 qmake最后生成什么文件。它的可选值分别是：
#app：可执行程序。
#lib：生成库。
#subdirs：依次构建子目录中的pro。子目录使用SUBDIRS变量指定。
#其它三个不常用：aux、vcapp、vclib
TEMPLATE  = subdirs

#考虑到依赖问题，按照SUBDIRS书写顺序来编译
CONFIG   += ordered

#SUBDIRS只有两个目录：src 和 share。按照顺序，应该是先编译 src，然后编译 share。
#src: 源代码文件。
#share: 源代码中所需要的一些非代码共享文件，例如代码模板等
SUBDIRS = src share

#对于 Unix 平台（unix），如果不是 Mac OS（!macx），并且copydata不为空（!isEmpty(copydata)），则需要再增加一个 bin 目录。
#copydata和BUILD_TESTS都是在 qtcreator.pri 中定义的宏
#bin目录是生成Linux平台shell脚本
#unix:!macx:!isEmpty(copydata):SUBDIRS += bin

#如果BUILD_TESTS不为空（!isEmpty(BUILD_TESTS)），则再增加一个 tests 目录
#tests: QtCreator测试代码
#!isEmpty(BUILD_TESTS):SUBDIRS += tests

#DESTFILES：将下列文件编译进最终目标文件中。
#$ $ file()不是变量展开，而是函数调用。
#qmake 提供了两类函数：替换函数（replace functions）和测试函数（test fucntion）。
#替换函数用于处理数据并将处理结果返回；测试函数的返回值只能是bool值，并且可以用于一些测试的情形。
#在使用时，替换函数需要添加$ $先导符而测试函数则不需要。
#$ $ file()正是一个替换函数，接受一个正则表达式作为参数，其返回值是所有符合这个正则表达式的文件名列表。
#因此,$ $file(dist/changes-*)返回的是在当前目录下的 dist 文件夹中，所有以 changes- 开头的文件，将它们全部添加到了DESTFILES。
#另外，这一函数还可以有第二个参数，是一个bool值，默认是false，表示是不是要递归寻找文件
DISTFILES += dist/copyright_template.txt \
    README.md \
    $$files(dist/changes-*) \
    qtcreator.qbs \
    $$files(qbs/*, true) \
    $$files(scripts/*.py) \
    $$files(scripts/*.sh) \
    $$files(scripts/*.pl)

#exists()就是一个测试函数，顾名思义，该函数用于测试其参数作为文件名，所代表的文件是否存在。
#测试函数的使用：它可以直接作为测试条件，后面跟着一对大括号，如果函数返回值为true则执行块中的语句。
exists(src/shared/qbs/qbs.pro) {
    # 确保qbs dll与Creator可执行文件一起结束。
    QBS_DLLDESTDIR = $${IDE_BUILD_TREE}/bin
    cache(QBS_DLLDESTDIR)
    QBS_DESTDIR = $${IDE_LIBRARY_PATH}
    cache(QBS_DESTDIR)
    QBSLIBDIR = $${IDE_LIBRARY_PATH}
    cache(QBSLIBDIR)
    QBS_INSTALL_PREFIX = $${QTC_PREFIX}
    cache(QBS_INSTALL_PREFIX)
    QBS_LIB_INSTALL_DIR = $$INSTALL_LIBRARY_PATH
    cache(QBS_LIB_INSTALL_DIR)
    QBS_RESOURCES_BUILD_DIR = $${IDE_DATA_PATH}/qbs
    cache(QBS_RESOURCES_BUILD_DIR)
    QBS_RESOURCES_INSTALL_DIR = $$INSTALL_DATA_PATH/qbs
    cache(QBS_RESOURCES_INSTALL_DIR)
    macx {
        QBS_PLUGINS_BUILD_DIR = $${IDE_PLUGIN_PATH}
        QBS_APPS_RPATH_DIR = @loader_path/../Frameworks
    } else {
        QBS_PLUGINS_BUILD_DIR = $$IDE_PLUGIN_PATH
        QBS_APPS_RPATH_DIR = \$\$ORIGIN/../$$IDE_LIBRARY_BASENAME/qtcreator
    }
    cache(QBS_PLUGINS_BUILD_DIR)
    cache(QBS_APPS_RPATH_DIR)
    QBS_PLUGINS_INSTALL_DIR = $$INSTALL_PLUGIN_PATH
    cache(QBS_PLUGINS_INSTALL_DIR)
    QBS_LIBRARY_DIRNAME = $${IDE_LIBRARY_BASENAME}
    cache(QBS_LIBRARY_DIRNAME)
    QBS_APPS_DESTDIR = $${IDE_BIN_PATH}
    cache(QBS_APPS_DESTDIR)
    QBS_APPS_INSTALL_DIR = $$INSTALL_BIN_PATH
    cache(QBS_APPS_INSTALL_DIR)
    QBS_LIBEXEC_DESTDIR = $${IDE_LIBEXEC_PATH}
    cache(QBS_LIBEXEC_DESTDIR)
    QBS_LIBEXEC_INSTALL_DIR = $$INSTALL_LIBEXEC_PATH
    cache(QBS_LIBEXEC_INSTALL_DIR)
    QBS_RELATIVE_LIBEXEC_PATH = $$relative_path($$QBS_LIBEXEC_DESTDIR, $$QBS_APPS_DESTDIR)
    isEmpty(QBS_RELATIVE_LIBEXEC_PATH):QBS_RELATIVE_LIBEXEC_PATH = .
    cache(QBS_RELATIVE_LIBEXEC_PATH)
    QBS_RELATIVE_PLUGINS_PATH = $$relative_path($$QBS_PLUGINS_BUILD_DIR, $$QBS_APPS_DESTDIR$$)
    cache(QBS_RELATIVE_PLUGINS_PATH)
    QBS_RELATIVE_SEARCH_PATH = $$relative_path($$QBS_RESOURCES_BUILD_DIR, $$QBS_APPS_DESTDIR)
    cache(QBS_RELATIVE_SEARCH_PATH)
    !qbs_no_dev_install {
        QBS_CONFIG_ADDITION = qbs_no_dev_install qbs_enable_project_file_updates
        cache(CONFIG, add, QBS_CONFIG_ADDITION)
    }

    # 创建qbs文档目标
    DOC_FILES =
    DOC_TARGET_PREFIX = qbs_
    include(src/shared/qbs/doc/doc_shared.pri)
    include(src/shared/qbs/doc/doc_targets.pri)
    docs.depends += qbs_docs
    !build_online_docs {
        install_docs.depends += install_qbs_docs
    }
    unset(DOC_FILES)
    unset(DOC_TARGET_PREFIX)
}

#contains()是一个测试函数，其函数原型是contains(variablename, value)，当变量variablename中包含了value时，测试通过。
#如果QT_ARCH中有i386，则将ARCHITECTURE赋值为x86，否则就是$ $QT_ARCH。
#注意在使用contains函数时，QT_ARCH并没有使用$$运算符。因为在使用该函数时，第一个参数是变量名，函数会自己取该变量名的实际值
contains(QT_ARCH, i386): ARCHITECTURE = x86
else: ARCHITECTURE = $$QT_ARCH

#定义了一个新的宏PLATFORM，根据平台赋值，实现跨平台编程
macx: PLATFORM = "mac"
else:win32: PLATFORM = "windows"
else:linux-*: PLATFORM = "linux-$${ARCHITECTURE}"
else: PLATFORM = "unknown"

#首先，我们定义了BASENAME宏为$ $(INSTALL_BASENAME)；
#如果BASENAME为空的话（使用了测试函数isEmpty()进行判断），则定义新的BASENAME的值。
#这种写法一方面允许我们在编译时通过传入自定义值改变默认设置（也就是说，如果之前定义了INSTALL_BASENAME，那么就会使用我们定义的值），
#否则就会生成一个默认值。以后我们会发现，Qt Creator 的 pro 文件中，很多地方都使用了类似的写法
BASENAME = $$(INSTALL_BASENAME)
isEmpty(BASENAME): BASENAME = qt-creator-$${PLATFORM}$(INSTALL_EDITION)-$${QTCREATOR_VERSION}$(INSTALL_POSTFIX)

linux {
    appstream.files = share/metainfo/org.qt-project.qtcreator.appdata.xml
    appstream.path = $$QTC_PREFIX/share/metainfo/

    desktop.files = share/applications/org.qt-project.qtcreator.desktop
    desktop.path = $$QTC_PREFIX/share/applications/

    INSTALLS += appstream desktop
}

macx {
    APPBUNDLE = "$$OUT_PWD/bin/$${IDE_APP_TARGET}.app"
    BINDIST_SOURCE.release = "$$OUT_PWD/bin/$${IDE_APP_TARGET}.app"
    BINDIST_SOURCE.debug = "$$OUT_PWD/bin"
    BINDIST_EXCLUDE_ARG.debug = "--exclude-toplevel"
    deployqt.commands = $$PWD/scripts/deployqtHelper_mac.sh \"$${APPBUNDLE}\" \"$$[QT_INSTALL_BINS]\" \"$$[QT_INSTALL_TRANSLATIONS]\" \"$$[QT_INSTALL_PLUGINS]\" \"$$[QT_INSTALL_IMPORTS]\" \"$$[QT_INSTALL_QML]\"
    codesign.commands = codesign --deep -o runtime -s \"$(SIGNING_IDENTITY)\" $(SIGNING_FLAGS) \"$${APPBUNDLE}\"
    dmg.commands = python -u \"$$PWD/scripts/makedmg.py\" \"$${BASENAME}.dmg\" \"Qt Creator\" \"$$IDE_SOURCE_TREE\" \"$$OUT_PWD/bin\"
    #dmg.depends = deployqt
    QMAKE_EXTRA_TARGETS += codesign dmg
} else {
    BINDIST_SOURCE.release = "$(INSTALL_ROOT)$$QTC_PREFIX"
    BINDIST_EXCLUDE_ARG.release = "--exclude-toplevel"
    BINDIST_SOURCE.debug = $${BINDIST_SOURCE.release}
    BINDIST_EXCLUDE_ARG.debug = $${BINDIST_EXCLUDE_ARG.release}
    deployqt.commands = python -u $$PWD/scripts/deployqt.py -i \"$(INSTALL_ROOT)$$QTC_PREFIX/bin/$${IDE_APP_TARGET}\" \"$(QMAKE)\"
    deployqt.depends = install
    # legacy dummy target
    win32: QMAKE_EXTRA_TARGETS += deployartifacts
}

INSTALLER_ARCHIVE_FROM_ENV = $$(INSTALLER_ARCHIVE)
isEmpty(INSTALLER_ARCHIVE_FROM_ENV) {
    INSTALLER_ARCHIVE = $$OUT_PWD/$${BASENAME}-installer-archive.7z
} else {
    INSTALLER_ARCHIVE = $$OUT_PWD/$$(INSTALLER_ARCHIVE)
}

INSTALLER_ARCHIVE_DEBUG = $$INSTALLER_ARCHIVE
INSTALLER_ARCHIVE_DEBUG ~= s/(.*)[.]7z/\1-debug.7z

bindist.commands = python -u $$PWD/scripts/createDistPackage.py $$OUT_PWD/$${BASENAME}.7z \"$${BINDIST_SOURCE.release}\"
bindist_installer.commands = python -u $$PWD/scripts/createDistPackage.py $${BINDIST_EXCLUDE_ARG.release} $${INSTALLER_ARCHIVE} \"$${BINDIST_SOURCE.release}\"
bindist_debug.commands = python -u $$PWD/scripts/createDistPackage.py --debug $${BINDIST_EXCLUDE_ARG.debug} $${INSTALLER_ARCHIVE_DEBUG} \"$${BINDIST_SOURCE.debug}\"

win32 {
    deployqt.commands ~= s,/,\\\\,g
    bindist.commands ~= s,/,\\\\,g
    bindist_installer.commands ~= s,/,\\\\,g
}

#打包配置 递归查询,并指定递归目录为src目录
deployqt.CONFIG += recursive
deployqt.recurse = src

#工程运行前执行的其他命令
QMAKE_EXTRA_TARGETS += deployqt bindist bindist_installer bindist_debug
