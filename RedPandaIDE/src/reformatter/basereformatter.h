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
#ifndef BASE_REFORMATTER_H
#define BASE_REFORMATTER_H
#include <functional>
#include <memory>
#include <QObject>

class BaseReformatter : public QObject {
    Q_OBJECT
public:
    BaseReformatter(QObject* parent = nullptr);
    virtual QString reformat(const QString& content, QString &errorMessage, bool &isOk) = 0;
    // A factory that builds an identical formatter from plain values, for the worker
    // thread that runs it: a QObject must be created and destroyed in the thread that
    // uses it, not in the GUI thread that starts the reformat. Empty when the
    // formatter has no such factory (the tests' formatters), in which case the worker
    // uses the formatter created on the GUI thread.
    virtual std::function<std::unique_ptr<BaseReformatter>()> workerFactory() const
    {
        return {};
    }
};

#endif
