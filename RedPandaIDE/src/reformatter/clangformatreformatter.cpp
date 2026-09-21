/*
 * Copyright (C) 2020-2026 Roy Qu (royqh1979@gmail.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "clangformatreformatter.h"
#include "../utils/escape.h"
#include "../utils/file.h"
#include "../utils.h"
#include <QByteArray>
#include <QDir>
#include <QFileInfo>

// clang-format is a single-shot filter, it should never run that long
static const int CLANG_FORMAT_TIMEOUT_MS = 15000;

ClangFormatReformatter::ClangFormatReformatter(const QString& clangFormatPath,
                                               const QStringList& args,
                                               const QString& fileName,
                                               const QString& baseDirectory,
                                               LoggerFunc newLoggerFunc,
                                               QObject *parent):
    BaseReformatter{parent},
    mClangFormatPath{clangFormatPath},
    mArgs{args},
    mFileName{fileName},
    mBaseDirectory{baseDirectory},
    mLoggerFunc{newLoggerFunc}
{

}

QString ClangFormatReformatter::refomat(const QString &content, QString &errorMessage, bool &isOk)
{
    isOk = false;
    if (!fileExists(mClangFormatPath)) {
        errorMessage = tr("Can't find clang-format in \"%1\".").arg(mClangFormatPath);
        return QString();
    }
    if (content.isEmpty()) {
        // clang-format doesn't output anything for empty input
        isOk = true;
        return QString();
    }
    if (!mFileName.isEmpty() && !isFormattableCppFileName(mFileName)) {
        // clang-format doesn't know the language of that file and would treat
        // it as c++ source; reformatting it would silently destroy its content.
        errorMessage = tr("clang-format can't format \"%1\".").arg(extractFileName(mFileName));
        return QString();
    }
    // the editor hands out the decoded text, so the utf-8 round trip used to
    // talk to clang-format must not lose any information
    QByteArray byteContent = content.toUtf8();
    if (QString::fromUtf8(byteContent) != content) {
        errorMessage = tr("The content of \"%1\" can't be converted to utf-8.").arg(mFileName);
        return QString();
    }

    // the directory that is used when the file has no usable name/path, and
    // that bounds the search for the ".clang-format" config file
    QString baseDirectory = mBaseDirectory;
    if (baseDirectory.isEmpty() || !fileExists(baseDirectory))
        baseDirectory = QDir::currentPath();

    QString fileName;
    QString workingDir;
    if (!mFileName.isEmpty()) {
        QFileInfo fileInfo(mFileName);
        // resolve relative names against baseDirectory; the current directory
        // of the process may have been changed at any time (see main.cpp)
        fileName = fileInfo.isAbsolute()? fileInfo.absoluteFilePath()
                                        : QDir(baseDirectory).absoluteFilePath(mFileName);
        QString fileDir = extractFileDir(fileName);
        workingDir = fileExists(fileDir)? fileDir : baseDirectory;
    } else {
        // unsaved file: format it as a new c++ file inside the base directory,
        // so that the config file search starts there. Using the folder where
        // clang-format is installed would make it walk up to the drive root.
        workingDir = baseDirectory;
        fileName = QDir(workingDir).absoluteFilePath("untitled.cpp");
    }

    QStringList args = mArgs;
    // clang-format uses the file name to detect the language and to search
    // for the ".clang-format" file
    bool hasAssumedFilename = false;
    for (const QString& arg: mArgs) {
        if (arg.startsWith("-assume-filename"))
            hasAssumedFilename = true;
    }
    // let the user override it through the extra command line options
    if (!hasAssumedFilename)
        args.append(QString("-assume-filename=%1").arg(fileName));

    QString command = escapeCommandForPlatformShell(extractFileName(mClangFormatPath), args);
    if (mLoggerFunc) {
        mLoggerFunc(tr("Reformatting content using clang-format..."));
        mLoggerFunc("------------------");
        mLoggerFunc(tr("- clang-format: %1").arg(mClangFormatPath));
        mLoggerFunc(tr("- Working dir: %1").arg(workingDir));
        mLoggerFunc(tr("- Command: %1").arg(command));
    }
    int exitCode = -1;
    auto [newContent, clangFormatError, processError] =
        runAndGetOutput(mClangFormatPath, workingDir, args, byteContent,
                        true, /*separateStderr*/
                        true, /*inheritEnvironment: clang-format needs a sane env */
                        QProcessEnvironment(),
                        CLANG_FORMAT_TIMEOUT_MS,
                        &exitCode);
    if (!processError.isEmpty()) {
        if (mLoggerFunc)
            mLoggerFunc(processError);
        errorMessage = processError;
        return QString();
    }
    QString errorOutput;
#ifdef Q_OS_WIN
    errorOutput = QString::fromLocal8Bit(clangFormatError);
#else
    errorOutput = QString::fromUtf8(clangFormatError);
#endif
    if (exitCode != 0) {
        // e.g. an invalid command line option, or no config file while the
        // fallback style is "none"
        if (errorOutput.isEmpty())
            errorMessage = tr("clang-format exits with code %1.").arg(exitCode);
        else
            errorMessage = errorOutput;
        if (mLoggerFunc)
            mLoggerFunc(errorMessage);
        return QString();
    }
    if (newContent.isEmpty()) {
        // clang-format writes nothing to the standard output if it fails
        if (!errorOutput.isEmpty())
            errorMessage = errorOutput;
        else
            errorMessage = tr("clang-format doesn't generate any output.");
        if (mLoggerFunc)
            mLoggerFunc(errorMessage);
        return QString();
    }
    if (!errorOutput.isEmpty() && mLoggerFunc) {
        // clang-format also writes warnings to the standard error
        mLoggerFunc(errorOutput);
    }
    isOk = true;
    return QString::fromUtf8(newContent);
}
