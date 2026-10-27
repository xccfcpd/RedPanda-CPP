#include "utils.h"
#include "systemconsts.h"
#include <QDate>
#include <QDateTime>
#include <QApplication>
#include <QDesktopServices>
#include <QSysInfo>
#include <QVersionNumber>
#include <QRandomGenerator>
#include <QOperatingSystemVersion>
#include <QtXml>
#include "editor.h"
#include "compiler/executablerunner.h"
#include <QComboBox>
#include "utils/escape.h"
#include "utils/parsearg.h"
#include "settings.h"
#ifdef Q_OS_WIN
#include <QDesktopServices>
#include <windows.h>
#endif

ProcessOutput runAndGetOutput(const QString &cmd, const QString& workingDir, const QStringList& arguments,
                           const QByteArray &inputContent,
                           bool separateStderr,
                           bool inheritEnvironment,
                           const QProcessEnvironment& env,
                           int timeoutMs,
                           int* exitCode)
{
    QProcess process;
    QByteArray standardOutput;
    QByteArray standardError;
    QString errorMessage;
    bool errorOccurred = false;
    if (env.isEmpty()) {
        if (inheritEnvironment) {
            process.setProcessEnvironment(QProcessEnvironment::systemEnvironment());
        } else {
            process.setProcessEnvironment(QProcessEnvironment());
        }
    } else {
        process.setProcessEnvironment(env);
    }
    if (separateStderr)
        process.setProcessChannelMode(QProcess::SeparateChannels);
    else
        process.setProcessChannelMode(QProcess::MergedChannels);
    process.setReadChannel(QProcess::StandardOutput);
    process.setWorkingDirectory(workingDir);
    process.connect(&process,&QProcess::readyReadStandardOutput,
                    [&](){
        standardOutput.append(process.readAllStandardOutput());
    });
    if (separateStderr)
        process.connect(&process, &QProcess::readyReadStandardError,
                        [&]() {
                            standardError.append(process.readAllStandardError());
                        });
    process.connect(&process, &QProcess::errorOccurred,
                    [&](){
                        errorOccurred= true;
                    });
    process.start(cmd,arguments);
    if (!inputContent.isEmpty()) {
        process.write(inputContent);
    }
    process.closeWriteChannel();
    // A negative timeout waits for the process to finish on its own: -1 is what asks
    // QProcess for that. The default timeout is bounded (see utils.h), because these
    // waits run on the GUI thread; only a caller that really needs it passes -1.
    bool timedout = false;
    if (timeoutMs >= 0) {
        if (!process.waitForFinished(timeoutMs)) {
            // Only a real timeout means the process is still running. A process that
            // failed to start or crashed also makes the wait fail, and it must be
            // reported (and handled) as that, not as a timeout.
            timedout = process.error() == QProcess::Timedout;
            if (timedout) {
                process.kill();
                // Don't let a process that ignores the kill add another 30 s to the
                // timeout: the caller asked for a bound, so the wait after the kill
                // is bounded too.
                process.waitForFinished(1000);
            }
        }
    } else {
        process.waitForFinished(-1);
    }
    // drain what's left, in case the signals above didn't deliver everything
    standardOutput.append(process.readAllStandardOutput());
    if (separateStderr)
        standardError.append(process.readAllStandardError());
    if (exitCode != nullptr)
        *exitCode = process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1;
    if (timedout) {
        errorMessage += QCoreApplication::translate(
                    "Utils", "Timeout: the process doesn't finish in %1 ms!")
                .arg(timeoutMs);
    } else if (errorOccurred) {
        switch(process.error()) {
        case QProcess::FailedToStart:
            errorMessage += QCoreApplication::translate(
                        "Utils", "Failed to start process!");
            break;
        case QProcess::Crashed:
            errorMessage += QCoreApplication::translate(
                        "Utils", "Process crashed!");
            break;
        case QProcess::Timedout:
            errorMessage += QCoreApplication::translate("Utils", "Timeout!");
            break;
        case QProcess::ReadError:
            errorMessage += QCoreApplication::translate("Utils", "Read Error:");
            break;
        case QProcess::WriteError:
            errorMessage += QCoreApplication::translate("Utils", "Write Error:");
            break;
        case QProcess::UnknownError:
            errorMessage += QCoreApplication::translate("Utils", "Unknown Error:");
            break;
        }
        errorMessage += process.errorString();
    }
    return {standardOutput, standardError, errorMessage};
}

void executeFile(const QString &fileName, const QStringList &params, const QString &workingDir, const QString &tempFile)
{
    ExecutableRunner* runner=new ExecutableRunner(
                fileName,
                params,
                workingDir);
    runner->connect(runner, &QThread::finished,
                    [runner,tempFile](){
        if (!tempFile.isEmpty()) {
            QFile::remove(tempFile);
        }
        runner->deleteLater();
    });
    runner->connect(runner, &Runner::runErrorOccurred,
            [](const QString&){
        //todo
    });
    runner->setStartConsole(true);
    runner->start();
}


qulonglong stringToHex(const QString &str, bool &isOk)
{
    qulonglong value = str.toULongLong(&isOk,16);
    return value;
}

QByteArray getHTTPBody(const QByteArray& content) {
    int i= content.indexOf("\r\n\r\n");
    if (i>=0) {
        return content.mid(i+4);
    }
    return "";
}

QString getSizeString(int size)
{
    if (size < 1024) {
        return QString("%1 ").arg(size)+QObject::tr("bytes");
    } else if (size < 1024 * 1024) {
        return QString("%1 ").arg(size / 1024.0,0,'f',2)+QObject::tr("KB");
    } else if (size < 1024 * 1024 * 1024) {
        return QString("%1 ").arg(size / 1024.0 / 1024.0)+QObject::tr("MB");
    } else {
        return QString("%1 ").arg(size / 1024.0 / 1024.0 / 1024.0)+QObject::tr("GB");
    }
}

void openFileFolderInExplorer(const QString &path)
{
    QFileInfo info(path);
    if (info.isFile()){
#ifdef Q_OS_WIN
        QProcess process;
        QStringList args;
        QString filepath=QDir::toNativeSeparators(info.absoluteFilePath());
        args.append("/n,");
        args.append("/select,");
        args.append(QString("%1").arg(filepath));
        process.startDetached("explorer.exe",args);
#else
        QDesktopServices::openUrl(
                    QUrl("file:///"+
                         includeTrailingPathDelimiter(info.path()),QUrl::TolerantMode));
#endif
    } else if (info.isDir()){
        QDesktopServices::openUrl(
                    QUrl("file:///"+
                         includeTrailingPathDelimiter(path),QUrl::TolerantMode));
    }
}


QStringList getExecutableSearchPaths()
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString path = env.value("PATH");
    QStringList pathList = path.split(PATH_SEPARATOR);
#ifdef Q_OS_WINDOWS
    /* follow Windows `CreateProcessW` search semantics.
     * ref. https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw .
     */
    QStringList searchList{};
    wchar_t buffer[MAX_PATH];
    // 1. the directory from which the application loaded
    searchList.push_back(QApplication::instance()->applicationDirPath());
    // 2. the current directory for the parent process
    // here we add it because launching from GUI the current directory is relatively stable
    searchList.push_back(QDir::currentPath());
    // 3. the 32-bit Windows system directory
    if (GetSystemDirectoryW(buffer, MAX_PATH) > 0)
        searchList.push_back(QString::fromWCharArray(buffer));
    if (GetWindowsDirectoryW(buffer, MAX_PATH) > 0) {
        // 4. the 16-bit Windows system directory
        searchList.push_back(QString::fromWCharArray(buffer) + "/System");
        // 5. the Windows directory
        searchList.push_back(QString::fromWCharArray(buffer));
    }
    // 6. the directories that are listed in the PATH environment variable
    searchList.append(pathList);
    return searchList;
#else
    return pathList;
#endif
}

bool programExists(const QString &program)
{
    if (program.isEmpty()) {
        return false;
    }
    if (program.contains('/') || program.contains(QDir::separator())) {
        return QFileInfo(program).isExecutable();
    }
    QStringList searchPaths = getExecutableSearchPaths();
    for (const QString &path : searchPaths) {
        QString filePath = path + QDir::separator() + program;
        if (QFileInfo(filePath).isExecutable()) {
            return true;
        }
    }
    return false;
}

QString findProgramInPath(const QString &program)
{
    if (program.isEmpty()) {
        return QString();
    }
    if (program.contains('/') || program.contains(QDir::separator())) {
        return QFileInfo(program).isExecutable() ? program : QString();
    }
    QStringList searchPaths = getExecutableSearchPaths();
    for (const QString &path : searchPaths) {
        if (path.isEmpty()) {
            continue;
        }
        QString filePath = path + QDir::separator() + program;
        if (QFileInfo(filePath).isExecutable()) {
            return filePath;
        }
    }
    return QString();
}

QString findBundledOrSystemTool(const QString &subDir, const QString &program)
{
    QString bundledPath = getFilePath(
        getFilePath(pSettings->dirs().appLibexecDir(), subDir),
        program
    );
    if (programExists(bundledPath))
        return bundledPath;

    // return as-is (search in PATH)
    return program;
}

QString localizeMakefilePath(const QString &path)
{
    if constexpr (MAKE_INTERFACE == MAKE_INTERFACE_mingw32) {
        QString result = path;
        result.replace("/",QDir::separator());
        return result;
    } else {
        return path;
    }
}

QStringList platformCommandForTerminalArgsPreview()
{
#ifdef Q_OS_WINDOWS
    QVersionNumber currentVersion = QVersionNumber::fromString(QSysInfo::kernelVersion());
    if (currentVersion >= QVersionNumber(6, 1))
        return {"powershell.exe", "-c", "echo hello; sleep 3"};
    else
        return {"cmd.exe", "/c", "echo hello & ping 127.0.0.1"};
#else
    return {"sh", "-c", "echo hello; sleep 3"};
#endif
}

QString byteArrayToString(const QByteArray &content, bool isUTF8)
{
    if (isUTF8)
        return QString::fromUtf8(content);
    else
        return QString::fromLocal8Bit(content);
}

QByteArray stringToByteArray(const QString &content, bool isUTF8)
{
    if (isUTF8)
        return content.toUtf8();
    else
        return content.toLocal8Bit();
}

void updateComboHistory(QStringList &historyList, const QString &newKey)
{
    int idx = historyList.indexOf(newKey);
    if (idx!=-1)
        historyList.removeAt(idx);
    if (!newKey.isEmpty())
        historyList.insert(0,newKey);
}

void setComboTextAndHistory(QComboBox *cb, const QString &newText, QStringList &historyList)
{
    int idx;
    if (!newText.isEmpty()) {
        idx = historyList.indexOf(newText);
        if (idx == -1) {
            historyList.insert(0, newText);
            idx = 0;
        }
    } else {
        idx = cb->currentIndex();
    }
    cb->clear();
    cb->addItems(historyList);
    cb->setCurrentIndex(idx);
}


