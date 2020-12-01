# 此pri文件用于部署在构建时未编译的文件
# Qt Creator. 如果使用影子构建，它将处理文件复制到构建目录中，并添加相应的安装目标。
# Qt Creator。它处理文件复制到构建目录(如果使用的话)
# 用法: 定义变量(下面详细介绍)，然后包括这个pri文件。
# STATIC_BASE         - STATIC_FILES中列出的文件的基本目录
# STATIC_FILES        - 要部署的文件列表
# STATIC_OUTPUT_BASE  - 编译输出中的基本目录
# STATIC_INSTALL_BASE - 安装输出中的基本目录

# 复制文件的自定义编译器
defineReplace(stripStaticBase) {
    return($$relative_path($$1, $$STATIC_BASE))
}

# 处理基于STATIC_BASE和STATIC_OUTPUT_BASE的条件复制
!isEmpty(STATIC_FILES) {
    isEmpty(STATIC_BASE): \
        error("Using STATIC_FILES without having STATIC_BASE set")
    isEmpty(STATIC_OUTPUT_BASE): \
        error("Using STATIC_FILES without having STATIC_OUTPUT_BASE set")
    !osx:isEmpty(STATIC_INSTALL_BASE): \
        error("Using STATIC_FILES without having STATIC_INSTALL_BASE set")

    !isEqual(STATIC_BASE, $$STATIC_OUTPUT_BASE) {
        copy2build.input += STATIC_FILES
        copy2build.output = $$STATIC_OUTPUT_BASE/${QMAKE_FUNC_FILE_IN_stripStaticBase}
        isEmpty(vcproj):copy2build.variable_out = PRE_TARGETDEPS
        win32:copy2build.commands = $$QMAKE_COPY \"${QMAKE_FILE_IN}\" \"${QMAKE_FILE_OUT}\"
        unix:copy2build.commands = $$QMAKE_COPY ${QMAKE_FILE_IN} ${QMAKE_FILE_OUT}
        copy2build.name = COPY ${QMAKE_FILE_IN}
        copy2build.CONFIG += no_link no_clean
        QMAKE_EXTRA_COMPILERS += copy2build
        for(static_file, STATIC_FILES) {
            QMAKE_DISTCLEAN += $$STATIC_OUTPUT_BASE/$$stripStaticBase($$static_file)
        }
    }

    static.files = $$STATIC_FILES
    static.base = $$STATIC_BASE
    static.path = $$STATIC_INSTALL_BASE
    INSTALLS += static
}
