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

#include "googletest.h"
#include "filesystem-utilities.h"

#include <refactoringcompilationdatabase.h>
#include <nativefilepath.h>

#include <utils/smallstring.h>

#include <utils/temporarydirectory.h>

using testing::Contains;
using testing::IsEmpty;
using testing::Not;
using testing::PrintToString;

namespace {

MATCHER_P3(IsCompileCommand,
           directory,
           filePath,
           commandLine,
           std::string(negation ? "isn't" : "is") + " compile command with directory "
               + PrintToString(directory) + ", file name " + PrintToString(filePath)
               + " and command line " + PrintToString(commandLine))
{
    if (arg.Directory != std::string(directory) || arg.Filename != std::string(filePath)
        || arg.CommandLine != commandLine)
        return false;

    return true;
}

class RefactoringCompilationDatabase : public ::testing::Test
{
protected:
    RefactoringCompilationDatabase()
    {
        database.addFile(ClangBackEnd::NativeFilePathView{temporarySourceFilePath},
                         {"cc", toNativePath(temporaryDirectoryPath + "/data.cpp"), "-DNO_DEBUG"});
    }

protected:
    ClangBackEnd::RefactoringCompilationDatabase database;
    Utils::SmallString temporaryDirectoryPath = QDir::toNativeSeparators(Utils::TemporaryDirectory::masterDirectoryPath());
    Utils::SmallString temporarySourceFilePath = QDir::toNativeSeparators(Utils::TemporaryDirectory::masterDirectoryPath() + "/data.cpp");

};

TEST_F(RefactoringCompilationDatabase, GetAllFilesContainsTranslationUnit)
{
    auto filePaths = database.getAllFiles();

    ASSERT_THAT(filePaths, Contains(std::string(temporarySourceFilePath)));
}

TEST_F(RefactoringCompilationDatabase, CompileCommandForFilePath)
{
    auto compileCommands = database.getAllCompileCommands();

    ASSERT_THAT(compileCommands,
                Contains(IsCompileCommand(temporaryDirectoryPath,
                                          toNativePath(temporaryDirectoryPath + "/data.cpp"),
                                          std::vector<std::string>{"cc",
                                                                   std::string(toNativePath(
                                                                       temporaryDirectoryPath
                                                                       + "/data.cpp")),
                                                                   "-DNO_DEBUG"})));
}

TEST_F(RefactoringCompilationDatabase, NoCompileCommandForFilePath)
{
    auto compileCommands = database.getAllCompileCommands();

    ASSERT_THAT(compileCommands,
                Not(Contains(IsCompileCommand(temporaryDirectoryPath,
                                              toNativePath(temporaryDirectoryPath + "/data.cpp2"),
                                              std::vector<std::string>{"cc",
                                                                       std::string(toNativePath(
                                                                           temporaryDirectoryPath
                                                                           + "/data.cpp")),
                                                                       "-DNO_DEBUG"}))));
}

TEST_F(RefactoringCompilationDatabase, FilePaths)
{
    auto filePaths = database.getAllFiles();

    ASSERT_THAT(filePaths, Contains(std::string(temporarySourceFilePath)));
}
} // namespace
