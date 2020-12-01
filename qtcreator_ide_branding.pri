#QTCREATOR_VERSION即 Qt Creator 的版本；
#QTCREATOR_COMPAT_VERSION是插件所兼容的 Qt Creator 版本；
#QTCREATOR_DISPLAY_VERSION即 Qt Creator 显示的版本；
#QTCREATOR_COPYRIGHT_YEAR即 Qt Creator 版权年份

#VERSION同样被赋值为 Qt Creator 的版本；
#BINARY_ARTIFACTS_BRANCH指定的是 git 的分支。
#值得说明的是VERSION。这其实是 qmake 定义的变量，当template是app时，用于指定应用程序的版本号；
#当template是lib时，用于指定库的版本号。
#在 Windows 平台，如果没有指定RC_FILE和RES_FILE变量，则会自动生成一个 .rc 文件。
#该文件包含FILEVERSION和PRODUCTVERSION两个字段。
#VERSION应该由主版本号、次版本号、补丁号和构建版本号等组成。每一项都是 0 到 65535 之间的整型。例如：
QTCREATOR_VERSION = 4.13.82
QTCREATOR_COMPAT_VERSION = 4.13.82
QTCREATOR_DISPLAY_VERSION = 4.14.0-beta1
QTCREATOR_COPYRIGHT_YEAR = 2020

#IDE_DISPLAY_NAME即 IDE的名称
#IDE_ID即 IDE的ID
#IDE_CASED_ID 即 IDE的包装ID
IDE_DISPLAY_NAME = Qt Creator
IDE_ID = qtcreator
IDE_CASED_ID = QtCreator

#产品包的组织
PRODUCT_BUNDLE_ORGANIZATION = org.qt-project

#项目用户文件扩展名
PROJECT_USER_FILE_EXTENSION = .user

IDE_DOC_FILES_ONLINE = $$PWD/doc/qtcreator/qtcreator-online.qdocconf \
                       $$PWD/doc/qtcreatordev/qtcreator-dev-online.qdocconf
IDE_DOC_FILES = $$PWD/doc/qtcreator/qtcreator.qdocconf \
                $$PWD/doc/qtcreatordev/qtcreator-dev.qdocconf
