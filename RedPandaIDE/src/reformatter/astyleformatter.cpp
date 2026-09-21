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
#include "astyleformatter.h"
#include "../utils/escape.h"
#include "../utils.h"
#include "../utils/file.h"
#include <QByteArray>
#include <QDir>
#include <QFileInfo>

// astyle is a single-shot filter, it should never run that long
static const int ASTYLE_TIMEOUT_MS = 15000;

AStyleReformatter::AStyleReformatter(const QString& astylePath, const QStringList& args,
                                     const QString& fileName, const QString& baseDirectory,
                                     LoggerFunc newLoggerFunc, QObject *parent):
    BaseReformatter{parent},
    mAstylePath{astylePath},
    mArgs{args},
    mFileName{fileName},
    mBaseDirectory{baseDirectory},
    mLoggerFunc{newLoggerFunc}
{

}

QString AStyleReformatter::refomat(const QString &content, QString &errorMessage, bool &isOk)
{
    isOk = false;
    if (!fileExists(mAstylePath)) {
        errorMessage = tr("Can't find astyle in \"%1\".").arg(mAstylePath);
        return QString();
    }
    if (content.isEmpty()) {
        // astyle doesn't output anything for empty input
        isOk = true;
        return QString();
    }
    if (!mFileName.isEmpty() && !isFormattableCppFileName(mFileName)) {
        // astyle doesn't know the language of that file and would treat it as
        // c++ source; reformatting it would silently destroy its content.
        errorMessage = tr("astyle can't format \"%1\".").arg(extractFileName(mFileName));
        return QString();
    }
    QByteArray byteContent = content.toUtf8();
    if (QString::fromUtf8(byteContent) != content) {
        errorMessage = tr("The content of \"%1\" can't be converted to utf-8.").arg(mFileName);
        return QString();
    }

    // astyle runs as a filter, so the working directory only matters for the
    // (rare) options that resolve a path, e.g. "--options=<file>". Resolve it the
    // same way ClangFormatReformatter does, so both formatters behave alike.
    QString baseDirectory = mBaseDirectory;
    if (baseDirectory.isEmpty() || !fileExists(baseDirectory))
        baseDirectory = QDir::currentPath();
    QString workingDir = baseDirectory;
    if (!mFileName.isEmpty()) {
        QFileInfo fileInfo(mFileName);
        QString fileName = fileInfo.isAbsolute()? fileInfo.absoluteFilePath()
                                                : QDir(baseDirectory).absoluteFilePath(mFileName);
        QString fileDir = extractFileDir(fileName);
        if (fileExists(fileDir))
            workingDir = fileDir;
    }

    QString command = escapeCommandForPlatformShell(extractFileName(mAstylePath), mArgs);
    if (mLoggerFunc) {
        mLoggerFunc(tr("Reformatting content using astyle..."));
        mLoggerFunc("------------------");
        mLoggerFunc(tr("- Astyle: %1").arg(mAstylePath));
        mLoggerFunc(tr("- Working dir: %1").arg(workingDir));
        mLoggerFunc(tr("- Command: %1").arg(command));
    }
    int exitCode = -1;
    auto [newContent, astyleError, processError] =
        runAndGetOutput(mAstylePath, workingDir, mArgs, byteContent,
                        true, /*separateStderr*/
                        true, /*inheritEnvironment: astyle needs a sane env */
                        QProcessEnvironment(),
                        ASTYLE_TIMEOUT_MS,
                        &exitCode);
    if (!processError.isEmpty()) {
        if (mLoggerFunc)
            mLoggerFunc(processError);
        errorMessage = processError;
        return QString();
    }
    QString errorOutput;
#ifdef Q_OS_WIN
    errorOutput = QString::fromLocal8Bit(astyleError);
#else
    errorOutput = QString::fromUtf8(astyleError);
#endif
    if (exitCode != 0) {
        // e.g. an invalid command line option
        if (errorOutput.isEmpty())
            errorMessage = tr("astyle exits with code %1.").arg(exitCode);
        else
            errorMessage = errorOutput;
        if (mLoggerFunc)
            mLoggerFunc(errorMessage);
        return QString();
    }
    if (newContent.isEmpty()) {
        // astyle writes nothing to the standard output if it fails
        if (!errorOutput.isEmpty())
            errorMessage = errorOutput;
        else
            errorMessage = tr("astyle doesn't generate any output.");
        if (mLoggerFunc)
            mLoggerFunc(errorMessage);
        return QString();
    }
    if (!errorOutput.isEmpty() && mLoggerFunc) {
        // astyle also writes warnings to the standard error
        mLoggerFunc(errorOutput);
    }
    isOk = true;
    return QString::fromUtf8(newContent);
}
