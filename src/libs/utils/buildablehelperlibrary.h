/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/
///可构建助手库
#pragma once

#include "environment.h"
#include "fileutils.h"

QT_FORWARD_DECLARE_CLASS(QFileInfo)

namespace Utils {

class QTCREATOR_UTILS_EXPORT BuildableHelperLibrary
{
public:
    //返回第一个qmake、qmake-qt4、qmake4的完整路径至少是版本2.0.0，因此是qt4 qmake

    static FilePath findSystemQt(const Environment &env);
    static FilePaths findQtsInEnvironment(const Environment &env, int maxCount = -1);
    static bool isQtChooser(const QFileInfo &info);
    static QString qtChooserToQmakePath(const QString &path);
    // 如果qmakePath上的qmake是Qt(由QtVersion使用)，则返回true
    static QString qtVersionForQMake(const QString &qmakePath);
    // 返回类似qmake4、qmake、qmake-qt4或所选择的发行版(由QtVersion使用)
    static QStringList possibleQMakeCommands();
    static QString filterForQmakeFileDialog();

    static QString byInstallDataHelper(const QString &sourcePath,
                                       const QStringList &sourceFileNames,
                                       const QStringList &installDirectories,
                                       const QStringList &validBinaryFilenames,
                                       bool acceptOutdatedHelper);

    static bool copyFiles(const QString &sourcePath, const QStringList &files,
                          const QString &targetDirectory, QString *errorMessage);

    struct BuildHelperArguments {
        QString helperName;
        QString directory;
        Environment environment;

        FilePath qmakeCommand;
        QString targetMode;
        FilePath mkspec;
        QString proFilename;
        QStringList qmakeArguments;

        QString makeCommand;
        QStringList makeArguments;
    };

    static bool buildHelper(const BuildHelperArguments &arguments,
                            QString *log, QString *errorMessage);

    static bool getHelperFileInfoFor(const QStringList &validBinaryFilenames,
                                     const QString &directory, QFileInfo* info);
};

}
