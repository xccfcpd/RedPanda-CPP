/*
 * Copyright (C) 2020-2022 Roy Qu (royqh1979@gmail.com)
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
#ifndef GDBMI_DEBUGGER_H
#define GDBMI_DEBUGGER_H

#include "debugger.h"
#include <QProcess>
#include <QByteArray>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <memory>
#include <QRegularExpression>

struct DebugCommand{
    QString command;
    QString params;
    DebugCommandSource source;
};

using PDebugCommand = std::shared_ptr<DebugCommand>;

class GDBMIDebuggerClient: public DebuggerClient {
    Q_OBJECT
public:
    explicit GDBMIDebuggerClient(Debugger* debugger, DebuggerType clientType, QObject *parent = nullptr);

    // DebuggerClient interface
public:
    void postCommand(const QString &Command, const QString &Params, DebugCommandSource Source = DebugCommandSource::Other) override;
    void registerInferiorStoppedCommand(const QString &Command, const QString &Params) override;
    void stopDebug() override;
    DebuggerType clientType() override;
    const PDebugCommand &currentCmd() const;

    void stepOver() override;
    void stepInto() override;
    void stepOut() override;
    void runTo(const QString& filename, int line) override;
    void resume() override;
    void stepOverInstruction() override;
    void stepIntoInstruction() override;
    void interrupt() override;

    void refreshStackVariables() override;

    void readMemory(const QString&  startAddress, int rows, int cols) override;
    void writeMemory(qulonglong address, unsigned char data) override;

    void addBreakpoint(PBreakpoint breakpoint) override;
    void removeBreakpoint(PBreakpoint breakpoint) override;
    void addWatchpoint(const QString& watchExp) override;
    void setBreakpointCondition(PBreakpoint breakpoint) override;

    void addWatch(const QString& expression) override;
    void removeWatch(PWatchVar watchVar) override;
    void writeWatchVar(const QString& varName, const QString& value) override;
    void refreshWatch(PWatchVar var) override;
    void refreshWatch() override;
    void fetchWatchVarChildren(const QString& varName) override;

    void evalExpression(const QString& expression) override;

    void refreshFrame() override;
    void refreshRegisters() override;
    void disassembleCurrentFrame(bool blendMode) override;
    void setDisassemblyLanguage(bool isIntel) override;
    // QThread interface
protected:
    void run() override;
    void runNextCmd() override;
private:
    QStringList tokenize(const QString& s) const;
    bool outputTerminated(const QByteArray& text) const;
    void handleBreakpoint(const GDBMIResultParser::ParseObject& breakpoint);
    void handleCreateVar(const GDBMIResultParser::ParseObject &multiVars);
    void handleFrame(const GDBMIResultParser::ParseValue &frame);
    void handleStack(const QList<GDBMIResultParser::ParseValue> & stack);
    void handleLocalVariables(const QList<GDBMIResultParser::ParseValue> & variables);
    void handleEvaluation(const QString& value);
    void handleMemory(const QList<GDBMIResultParser::ParseValue> & rows);
    void handleRegisterNames(const QList<GDBMIResultParser::ParseValue> & names);
    void handleRegisterValue(const QList<GDBMIResultParser::ParseValue> & values);
    void handleListVarChildren(const GDBMIResultParser::ParseObject& multiVars);
    void handleUpdateVarValue(const QList<GDBMIResultParser::ParseValue> &changes);
    void processConsoleOutput(const QByteArray& line);
    void processLogOutput(const QByteArray& line);
    void processResult(const QByteArray& result);
    void processExecAsyncRecord(const QByteArray& line);
    void processError(const QByteArray& errorLine);
    void processResultRecord(const QByteArray& line);
    void processDebugOutput(const QByteArray& debugOutput);
    QByteArray removeToken(const QByteArray& line) const;
    void runInferiorStoppedHook();
    void clearCmdQueue();
private slots:
    void asyncUpdate();
private:
    bool mStop;
    std::shared_ptr<QProcess> mProcess;
    QMap<QString,QStringList> mFileCache;
    int mCurrentLine;
    qulonglong mCurrentAddress;
    QString mCurrentFunc;
    QString mCurrentFile;

    bool mAsyncUpdated;

    static const QRegularExpression REGdbSourceLine;

    QQueue<PDebugCommand> mCmdQueue;
    PDebugCommand mCurrentCmd;
    QList<PDebugCommand> mInferiorStoppedHookCommands;

    DebuggerType mClientType;
};

#endif // GDBMI_DEBUGGER_H