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
#ifndef CLANG_FORMAT_REFORMATTER_H
#define CLANG_FORMAT_REFORMATTER_H
#include "basereformatter.h"
#include "../utils/types.h"

class ClangFormatReformatter : public BaseReformatter {
    Q_OBJECT
public:
    // baseDirectory is used as the working directory, and as the reference
    // directory for the file name, when the edited file has no (absolute) name
    // yet. It decides where the search for the nearest ".clang-format" starts.
    ClangFormatReformatter(const QString& clangFormatPath,
                           const QStringList& args,
                           const QString& fileName,
                           const QString& baseDirectory,
                           LoggerFunc newLoggerFunc,
                           QObject* parent = nullptr);
    QString refomat(const QString& content, QString &errorMessage, bool &isOk) override;
private:
    QString mClangFormatPath;
    QStringList mArgs;
    QString mFileName;
    QString mBaseDirectory;
    LoggerFunc mLoggerFunc;
};

#endif
