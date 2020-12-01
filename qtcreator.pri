#如果存在QTCREATOR_PRI_INCLUDED，则抛出错误。
#下面一行则设置了QTCREATOR_PRI_INCLUDED。
#这两行类防止将 qtcreator.pri 引入多次
!isEmpty(QTCREATOR_PRI_INCLUDED):error("qtcreator.pri already included")
QTCREATOR_PRI_INCLUDED = 1

include($$PWD/qtcreator_ide_branding.pri)
!isEmpty(IDE_BRANDING_PRI): include($$IDE_BRANDING_PRI)

PRODUCT_BUNDLE_IDENTIFIER=$${PRODUCT_BUNDLE_ORGANIZATION}.$${IDE_ID}
VERSION = $$QTCREATOR_VERSION

#包括c++1z作为c++17的别名，以兼容较旧的Qt版本,我们在sdktool中使用的
CONFIG += c++17 c++1z

#下面是两个重要的函数定义。这两个函数都是用于生成库名字的
#下面来看qtLibraryTargetName是如何定义的。首先，取消LIBRARY_NAME的定义，
#然后使用语句LIBRARY_NAME = $$1赋值。定义函数时可以有参数，使用$$1即获取该参数。
#我们将$$1，也就是第一个参数，赋值给变量LIBRARY_NAME。下面是CONFIG测试函数，
#该函数可以用于检测CONFIG变量中的值。CONFIG是一个重要变量，用于指定编译参数，例如我们可以在这里设置 debug 或 release；
defineReplace(qtLibraryTargetName) {
   unset(LIBRARY_NAME)
   LIBRARY_NAME = $$1
   CONFIG(debug, debug|release) {
      !debug_and_release|build_pass {
          mac:RET = $$member(LIBRARY_NAME, 0)_debug
              else:win32:RET = $$member(LIBRARY_NAME, 0)d
      }
   }
   isEmpty(RET):RET = $$LIBRARY_NAME
   return($$RET)
}

#qtLibraryName替换函数与此类似。首先，它会获得qtLibraryTargetName的返回值。
#如果 win32 环境下，使用split将前面定义的QTCREATOR_VERSION以.为分隔符分成列表，
#然后使用$$RET$$first(VERSION_LIST)语句，将RET与VERSION_LIST的第一个元素，也就是 major 值，拼接后返回。
#这是因为，在 Windows 平台，为了避免出现 dll hell，Qt 会自动为生成的 dll 增加版本号。
#而我们一般只需要主版本一致即可，所以会重新生成新的名字。
#dll hell 只发生在 Windows 平台，因此这里只需要判断 win32 即可
defineReplace(qtLibraryName) {
   RET = $$qtLibraryTargetName($$1)
   win32 {
      VERSION_LIST = $$split(QTCREATOR_VERSION, .)
      RET = $$RET$$first(VERSION_LIST)
   }
   return($$RET)
}

#定义qt最低版本检查函数minQtVersion,小于指定版本返回false
defineTest(minQtVersion) {
    maj = $$1
    min = $$2
    patch = $$3
    isEqual(QT_MAJOR_VERSION, $$maj) {
        isEqual(QT_MINOR_VERSION, $$min) {
            isEqual(QT_PATCH_VERSION, $$patch) {
                return(true)
            }
            greaterThan(QT_PATCH_VERSION, $$patch) {
                return(true)
            }
        }
        greaterThan(QT_MINOR_VERSION, $$min) {
            return(true)
        }
    }
    greaterThan(QT_MAJOR_VERSION, $$maj) {
        return(true)
    }
    return(false)
}

#用于自定义编译器复制文件。函数absolute_path(path[, base])返回参数path的绝对路径；
#如果没有传入base，则将当前目录作为path的 base。与此类似，函数relative_path(filePath[, base])返回参数path的相对路径。
#有关“PWD”的几个内置变量是非常常用的。很多时候，我们希望在构建时自动复制一些文件到目标路径，往往需要使用这些变量。
#这些变量有：
#PWD	使用该变量PWD的文件（.pro 文件或者 .pri 文件）所在目录。
#_PRO_FILE_PWD_	    .pro 文件所在目录；即使该变量出现在 .pri 文件，也是指包含该 .pri 文件的 .pro 文件所在目录。
#_PRO_FILE_	.pro 文件完整路径。
#OUT_PWD	生成的 makefile 所在目录。
defineReplace(stripSrcDir) {
    return($$relative_path($$absolute_path($$1, $$OUT_PWD), $$_PRO_FILE_PWD_))
}

darwin:!minQtVersion(5, 7, 0) {
    # Qt 5.6 仍然设置部署目标10.7，它不能工作
    # c++ 11/14的所有特性(例如std::future)
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.8
}

QTC_BUILD_TESTS = $$(QTC_BUILD_TESTS)
!isEmpty(QTC_BUILD_TESTS):TEST = $$QTC_BUILD_TESTS

!isEmpty(BUILD_TESTS):TEST = 0

isEmpty(TEST):CONFIG(debug, debug|release) {
    !debug_and_release|build_pass {
        TEST = 1
    }
}

isEmpty(IDE_LIBRARY_BASENAME) {
    IDE_LIBRARY_BASENAME = lib
}

equals(TEST, 1) {
    QT +=testlib
    DEFINES += WITH_TESTS
}

#IDE_SOURCE_TREE即源代码所在目录。注意我们使用了$$PWD变量直接赋值：
#这个值会随着 .pro 文件或 .pri 文件的不同位置而有所不同，但都应由此找到源代码树。这是目录组织中需要注意的问题。
#函数re_escape(string)将参数string中出现的所有正则表达式中的保留字进行转义。
#例如，()是正则表达式的保留字，那么，$$re_escape(f(x))的返回值将是f\\(x\\)。
#这一函数的目的是保证获得一个合法的正则表达式。函数re_escape(path)将参数path中的.以及..等占位符移除，获得一个明确的路径。
IDE_SOURCE_TREE = $$PWD
isEmpty(IDE_BUILD_TREE) {
    sub_dir = $$_PRO_FILE_PWD_
    sub_dir ~= s,^$$re_escape($$PWD),,
    IDE_BUILD_TREE = $$clean_path($$OUT_PWD)
    IDE_BUILD_TREE ~= s,$$re_escape($$sub_dir)$,,
}

#设置了最终输出的二进制文件的位置，也就是在根目录下的 bin 目录中。
#接下来很多行都是设置目录位置，因为语法都很简单，这里不再详细介绍。
#可以看出，Qt Creator 编译过程中所有的输出位置，都是基于IDE_BUILD_TREE这个变量。
#由此顺利组织我们所需要的输出文件夹树，是很有用的
IDE_APP_PATH = $$IDE_BUILD_TREE/bin
osx {
    IDE_APP_TARGET   = "$$IDE_DISPLAY_NAME"

#检查IDE_BUILD_TREE是否确实是一个现有的Qt Creator.app，
#用于根据二进制包进行构建
    exists($$IDE_BUILD_TREE/Contents/MacOS/$$IDE_APP_TARGET): IDE_APP_BUNDLE = $$IDE_BUILD_TREE
    else: IDE_APP_BUNDLE = $$IDE_APP_PATH/$${IDE_APP_TARGET}.app

    # 如果没有手动设置，则设置输出路径
    isEmpty(IDE_OUTPUT_PATH): IDE_OUTPUT_PATH = $$IDE_APP_BUNDLE/Contents

    IDE_LIBRARY_PATH = $$IDE_OUTPUT_PATH/Frameworks
    IDE_PLUGIN_PATH  = $$IDE_OUTPUT_PATH/PlugIns
    IDE_LIBEXEC_PATH = $$IDE_OUTPUT_PATH/Resources/libexec
    IDE_DATA_PATH    = $$IDE_OUTPUT_PATH/Resources
    IDE_DOC_PATH     = $$IDE_DATA_PATH/doc
    IDE_BIN_PATH     = $$IDE_OUTPUT_PATH/MacOS
    copydata = 1

    LINK_LIBRARY_PATH = $$IDE_APP_BUNDLE/Contents/Frameworks
    LINK_PLUGIN_PATH  = $$IDE_APP_BUNDLE/Contents/PlugIns

    INSTALL_LIBRARY_PATH = $$QTC_PREFIX/$${IDE_APP_TARGET}.app/Contents/Frameworks
    INSTALL_PLUGIN_PATH  = $$QTC_PREFIX/$${IDE_APP_TARGET}.app/Contents/PlugIns
    INSTALL_LIBEXEC_PATH = $$QTC_PREFIX/$${IDE_APP_TARGET}.app/Contents/Resources/libexec
    INSTALL_DATA_PATH    = $$QTC_PREFIX/$${IDE_APP_TARGET}.app/Contents/Resources
    INSTALL_DOC_PATH     = $$INSTALL_DATA_PATH/doc
    INSTALL_BIN_PATH     = $$QTC_PREFIX/$${IDE_APP_TARGET}.app/Contents/MacOS
    INSTALL_APP_PATH     = $$QTC_PREFIX/
} else {
    contains(TEMPLATE, vc.*):vcproj = 1
    IDE_APP_TARGET   = $$IDE_ID

    # 目标输出路径(如果没有手动设置)
    isEmpty(IDE_OUTPUT_PATH): IDE_OUTPUT_PATH = $$IDE_BUILD_TREE

    IDE_LIBRARY_PATH = $$IDE_OUTPUT_PATH/$$IDE_LIBRARY_BASENAME/qtcreator
    IDE_PLUGIN_PATH  = $$IDE_LIBRARY_PATH/plugins
    IDE_DATA_PATH    = $$IDE_OUTPUT_PATH/share/qtcreator
    IDE_DOC_PATH     = $$IDE_OUTPUT_PATH/share/doc/qtcreator
    IDE_BIN_PATH     = $$IDE_OUTPUT_PATH/bin
    win32: \
        IDE_LIBEXEC_PATH = $$IDE_OUTPUT_PATH/bin
    else: \
        IDE_LIBEXEC_PATH = $$IDE_OUTPUT_PATH/libexec/qtcreator
    !isEqual(IDE_SOURCE_TREE, $$IDE_OUTPUT_PATH):copydata = 1

    LINK_LIBRARY_PATH = $$IDE_BUILD_TREE/$$IDE_LIBRARY_BASENAME/qtcreator
    LINK_PLUGIN_PATH  = $$LINK_LIBRARY_PATH/plugins

    INSTALL_LIBRARY_PATH = $$QTC_PREFIX/$$IDE_LIBRARY_BASENAME/qtcreator
    INSTALL_PLUGIN_PATH  = $$INSTALL_LIBRARY_PATH/plugins
    win32: \
        INSTALL_LIBEXEC_PATH = $$QTC_PREFIX/bin
    else: \
        INSTALL_LIBEXEC_PATH = $$QTC_PREFIX/libexec/qtcreator
    INSTALL_DATA_PATH    = $$QTC_PREFIX/share/qtcreator
    INSTALL_DOC_PATH     = $$QTC_PREFIX/share/doc/qtcreator
    INSTALL_BIN_PATH     = $$QTC_PREFIX/bin
    INSTALL_APP_PATH     = $$QTC_PREFIX/bin
}

gcc:!clang: QMAKE_CXXFLAGS += -Wno-noexcept-type

RELATIVE_PLUGIN_PATH = $$relative_path($$IDE_PLUGIN_PATH, $$IDE_BIN_PATH)
RELATIVE_LIBEXEC_PATH = $$relative_path($$IDE_LIBEXEC_PATH, $$IDE_BIN_PATH)
RELATIVE_DATA_PATH = $$relative_path($$IDE_DATA_PATH, $$IDE_BIN_PATH)
RELATIVE_DOC_PATH = $$relative_path($$IDE_DOC_PATH, $$IDE_BIN_PATH)
DEFINES += $$shell_quote(RELATIVE_PLUGIN_PATH=\"$$RELATIVE_PLUGIN_PATH\")
DEFINES += $$shell_quote(RELATIVE_LIBEXEC_PATH=\"$$RELATIVE_LIBEXEC_PATH\")
DEFINES += $$shell_quote(RELATIVE_DATA_PATH=\"$$RELATIVE_DATA_PATH\")
DEFINES += $$shell_quote(RELATIVE_DOC_PATH=\"$$RELATIVE_DOC_PATH\")

INCLUDEPATH += \
    $$IDE_BUILD_TREE/src \ # < app / app_version。h>在实际构建目录的情况下
    $$IDE_SOURCE_TREE/src \ # < app / app_version。h>的情况下二进制包与开发包
    $$IDE_SOURCE_TREE/src/libs \
    $$IDE_SOURCE_TREE/tools

win32:exists($$IDE_SOURCE_TREE/lib/qtcreator) {
    # 对于带有开发包的二进制包。lib
    LIBS *= -L$$IDE_SOURCE_TREE/lib/qtcreator
    LIBS *= -L$$IDE_SOURCE_TREE/lib/qtcreator/plugins
}

QTC_PLUGIN_DIRS_FROM_ENVIRONMENT = $$(QTC_PLUGIN_DIRS)
QTC_PLUGIN_DIRS += $$split(QTC_PLUGIN_DIRS_FROM_ENVIRONMENT, $$QMAKE_DIRLIST_SEP)
QTC_PLUGIN_DIRS += $$IDE_SOURCE_TREE/src/plugins
for(dir, QTC_PLUGIN_DIRS) {
    INCLUDEPATH += $$dir
}

QTC_LIB_DIRS_FROM_ENVIRONMENT = $$(QTC_LIB_DIRS)
QTC_LIB_DIRS += $$split(QTC_LIB_DIRS_FROM_ENVIRONMENT, $$QMAKE_DIRLIST_SEP)
QTC_LIB_DIRS += $$IDE_SOURCE_TREE/src/libs
QTC_LIB_DIRS += $$IDE_SOURCE_TREE/src/libs/3rdparty
for(dir, QTC_LIB_DIRS) {
    INCLUDEPATH += $$dir
}

CONFIG += \
    depend_includepath \
    no_include_pwd

LIBS *= -L$$LINK_LIBRARY_PATH  # Qt Creator库
exists($$IDE_LIBRARY_PATH): LIBS *= -L$$IDE_LIBRARY_PATH  # 库路径从输出路径

!isEmpty(vcproj) {
    DEFINES += IDE_LIBRARY_BASENAME=\"$$IDE_LIBRARY_BASENAME\"
} else {
    DEFINES += IDE_LIBRARY_BASENAME=\\\"$$IDE_LIBRARY_BASENAME\\\"
}

DEFINES += \
    QT_CREATOR \
    QT_NO_JAVA_STYLE_ITERATORS \
    QT_NO_CAST_TO_ASCII \
    QT_RESTRICTED_CAST_FROM_ASCII \
    QT_DISABLE_DEPRECATED_BEFORE=0x050900 \
    QT_USE_QSTRINGBUILDER

unix {
    CONFIG(debug, debug|release):OBJECTS_DIR = $${OUT_PWD}/.obj/debug-shared
    CONFIG(release, debug|release):OBJECTS_DIR = $${OUT_PWD}/.obj/release-shared

    CONFIG(debug, debug|release):MOC_DIR = $${OUT_PWD}/.moc/debug-shared
    CONFIG(release, debug|release):MOC_DIR = $${OUT_PWD}/.moc/release-shared

    RCC_DIR = $${OUT_PWD}/.rcc
    UI_DIR = $${OUT_PWD}/.uic
}

msvc {
    #不要警告sprintf, fopen等是“不安全的”
    DEFINES += _CRT_SECURE_NO_WARNINGS
    QMAKE_CXXFLAGS_WARN_ON *= -w44996
    # 加速cdb调试时的启动时间
    QMAKE_LFLAGS_DEBUG += /INCREMENTAL:NO
}

qt {
    contains(QT, core): QT += concurrent
    contains(QT, core): greaterThan(QT_MAJOR_VERSION, 5): QT += core5compat
    contains(QT, gui): QT += widgets
}

QBSFILE = $$replace(_PRO_FILE_, \\.pro$, .qbs)
exists($$QBSFILE):DISTFILES += $$QBSFILE

#LIBS是 qmake 连接第三方库的配置。-L指定了第三方库所在的目录；-l指定了第三方库的名字。如果没有-l，则会连接-L指定的目录中所有的库。
!isEmpty(QTC_PLUGIN_DEPENDS) {
    LIBS *= -L$$IDE_PLUGIN_PATH  # 插件路径从输出目录
    LIBS *= -L$$LINK_PLUGIN_PATH  # 当输出路径与Qt Creator构建目录不同时
}

#递归处理插件依赖。ever是一个常量，永远不会为false或空值，因此for(ever)是无限循环，直到使用break()跳出。
#如果QTC_PLUGIN_DEPENDS为空，则直接退出循环。事实上，QTC_PLUGIN_DEPENDS默认没有设置，所以这段代码其实并没有使用。
#这段代码的目的是，允许用户在编译时直接通过QTC_PLUGIN_DEPENDS指定插件依赖。如果没有，则根据每个插件自己的依赖处理。
#后面的for循环遍历QTC_PLUGIN_DEPENDS中指定的每一个依赖，
#然后用另外一个嵌套的循环遍历$$QTC_PLUGIN_DIRS指定的插件目录中的每一个目录，找出对应的插件，取其对应的 .pri 文件，即dependencies_file。
#注意循环的最后，使用-=运算符移除每次处理的依赖，直到最后QTC_PLUGIN_DEPENDS为空，退出循环
done_plugins =
for(ever) {
    isEmpty(QTC_PLUGIN_DEPENDS): \
        break()
    done_plugins += $$QTC_PLUGIN_DEPENDS
    for(dep, QTC_PLUGIN_DEPENDS) {
        dependencies_file =
        for(dir, QTC_PLUGIN_DIRS) {
            exists($$dir/$$dep/$${dep}_dependencies.pri) {
                dependencies_file = $$dir/$$dep/$${dep}_dependencies.pri
                break()
            }
        }
        isEmpty(dependencies_file): \
            error("Plugin dependency $$dep not found")
        include($$dependencies_file)
        LIBS += -l$$qtLibraryName($$QTC_PLUGIN_NAME)
    }
    QTC_PLUGIN_DEPENDS = $$unique(QTC_PLUGIN_DEPENDS)
    QTC_PLUGIN_DEPENDS -= $$unique(done_plugins)
}

# 递归解析所有依赖库
done_libs =
for(ever) {
    isEmpty(QTC_LIB_DEPENDS): \
        break()
    done_libs += $$QTC_LIB_DEPENDS
    for(dep, QTC_LIB_DEPENDS) {
        dependencies_file =
        for(dir, QTC_LIB_DIRS) {
            exists($$dir/$$dep/$${dep}_dependencies.pri) {
                dependencies_file = $$dir/$$dep/$${dep}_dependencies.pri
                break()
            }
        }
        isEmpty(dependencies_file): \
            error("Library dependency $$dep not found")
        include($$dependencies_file)
        LIBS += -l$$qtLibraryName($$QTC_LIB_NAME)
    }
    QTC_LIB_DEPENDS = $$unique(QTC_LIB_DEPENDS)
    QTC_LIB_DEPENDS -= $$unique(done_libs)
}
