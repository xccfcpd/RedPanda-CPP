// A minimal Settings stub for the tests that link BreakpointModel: the model only
// uses pSettings->debugger() for the font of its view, which these tests never ask
// for. The stub keeps the real model testable without pulling the whole application
// settings implementation in.
#include "src/settings.h"
#include "src/settings/basesettings.h"
#include "src/settings/debuggersettings.h"
#include "src/utils.h"

#include <QProcess>

Settings* pSettings = nullptr;

// Used by MemoryModel (debuggermodels.cpp); the rest of utils.cpp is not needed by
// the breakpoint tests.
qulonglong stringToHex(const QString& str, bool& isOk)
{
    return str.toULongLong(&isOk, 16);
}

// The subset of utils.cpp the real formatters need: they run their executable as a
// pipe. The full utils.cpp pulls in the compiler runner and the application settings,
// which these tests don't need.
ProcessOutput runAndGetOutput(const QString& cmd, const QString& workingDir,
                              const QStringList& arguments, const QByteArray& inputContent,
                              bool separateStderr, bool inheritEnvironment,
                              const QProcessEnvironment& env, int timeoutMs, int* exitCode)
{
    QProcess process;
    if (env.isEmpty()) {
        if (inheritEnvironment)
            process.setProcessEnvironment(QProcessEnvironment::systemEnvironment());
        else
            process.setProcessEnvironment(QProcessEnvironment());
    } else {
        process.setProcessEnvironment(env);
    }
    process.setProcessChannelMode(separateStderr ? QProcess::SeparateChannels
                                                 : QProcess::MergedChannels);
    process.setWorkingDirectory(workingDir);
    process.start(cmd, arguments);
    if (!inputContent.isEmpty())
        process.write(inputContent);
    process.closeWriteChannel();
    bool timedout = false;
    if (timeoutMs >= 0) {
        if (!process.waitForFinished(timeoutMs)) {
            timedout = process.error() == QProcess::Timedout;
            if (timedout) {
                process.kill();
                process.waitForFinished(1000);
            }
        }
    } else {
        process.waitForFinished(-1);
    }
    QByteArray output = process.readAllStandardOutput();
    QByteArray errorOutput = separateStderr ? process.readAllStandardError() : QByteArray();
    if (exitCode)
        *exitCode = process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1;
    QString errorMessage;
    if (timedout)
        errorMessage = QStringLiteral("timeout");
    else if (process.exitStatus() != QProcess::NormalExit)
        errorMessage = process.errorString();
    return ProcessOutput{output, errorOutput, errorMessage};
}

DebuggerSettings& Settings::debugger()
{
    static SettingsPersistor persistor(QStringLiteral("test-debugger.ini"));
    static DebuggerSettings settings(&persistor);
    return settings;
}
