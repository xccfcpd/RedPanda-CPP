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
#ifndef ASTYLE_REFORMATTER_H
#define ASTYLE_REFORMATTER_H
#include "basereformatter.h"
#include "../utils/types.h"

class AStyleReformatter : public BaseReformatter {
    Q_OBJECT
public:
    // fileName/baseDirectory are only used to pick a sensible working directory
    // (mirroring ClangFormatReformatter); astyle itself is run as a filter.
    AStyleReformatter(const QString& astylePath, const QStringList& args,
                      const QString& fileName, const QString& baseDirectory,
                      LoggerFunc newLoggerFunc, QObject* parent = nullptr);
    QString reformat(const QString& content, QString &errorMessage, bool &isOk) override;
private:
    QString mAstylePath;
    QStringList mArgs;
    QString mFileName;
    QString mBaseDirectory;
    LoggerFunc mLoggerFunc;
};

#endif
