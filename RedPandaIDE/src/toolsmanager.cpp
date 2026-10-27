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
#include "toolsmanager.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStringList>
#include <QJsonObject>
#include <QMessageBox>
#include <QUuid>
#include "settings.h"
#include "systemconsts.h"

namespace {

// Defaults of the built-in "Remove Compiled" tool. They are used to create the
// tool, and to migrate a configuration written before the isBuiltIn field existed.
#ifdef Q_OS_WIN
const char* const REMOVE_COMPILED_PROGRAM = "del";
const char* const REMOVE_COMPILED_PARAMETERS = "/q /f <EXENAME>";
#else
const char* const REMOVE_COMPILED_PROGRAM = "rm";
const char* const REMOVE_COMPILED_PARAMETERS = "-f <EXENAME>";
#endif
const char* const REMOVE_COMPILED_WORKDIR = "<SOURCEPATH>";

// True when "item" is (still) the built-in "Remove Compiled" tool. Its title is
// persisted translated, so it is recognized by its command instead of its name.
bool isRemoveCompiledTool(const PToolItem& item)
{
    return item->workingDirectory == QLatin1String(REMOVE_COMPILED_WORKDIR)
            && item->parameters == QLatin1String(REMOVE_COMPILED_PARAMETERS)
            && item->program == QLatin1String(REMOVE_COMPILED_PROGRAM);
}

// The title the built-in tool was persisted with, in every language the application
// ships a translation for. An old configuration has no isBuiltIn field: its title is
// the only marker that tells it from a user tool with the same command.
bool isKnownRemoveCompiledTitle(const QString& title)
{
    static const QStringList titles{
        QStringLiteral("Remove Compiled"),
        QStringLiteral("删除编译文件"),
        QStringLiteral("刪除編譯檔案"),
        QStringLiteral("Удалить скомпилированное"),
        QStringLiteral("Remover compilado")
    };
    return titles.contains(title);
}

} // namespace

ToolsManager::ToolsManager(QObject *parent) : QObject(parent)
{

}

void ToolsManager::load()
{
    //if config file not exists, copy it from data
    QString filename = includeTrailingPathDelimiter(pSettings->dirs().config()) + DEV_TOOLS_FILE;
    if (!fileExists(filename)) {
        mTools.clear();
        PToolItem item = std::make_shared<ToolItem>();
        item->id = QUuid::createUuid().toString();
        item->title = tr("Remove Compiled");
        item->program = REMOVE_COMPILED_PROGRAM;
        item->workingDirectory = REMOVE_COMPILED_WORKDIR;
        item->parameters = REMOVE_COMPILED_PARAMETERS;
        item->inputOrigin = ToolItemInputOrigin::None;
        item->outputTarget = ToolItemOutputTarget::RedirectToToolsOutputPanel;
        item->isUTF8 = false;
        item->isBuiltIn = true;
        mTools.append(item);
//#ifdef Q_OS_WIN
//        item = std::make_shared<ToolItem>();
//        item->title = tr("Open compiled in explorer");
//        item->program = "explorer.exe";
//        item->workingDirectory = "<SOURCEPATH>";
//        item->parameters = " /n, /select, <EXENAME>";
//        item->pauseAfterExit = false;
//        mTools.append(item);
//#endif
        save();
        return;
    }
    //read config file
    QFile file(filename);
    if (!file.open(QFile::ReadOnly)) {
        QMessageBox::critical(nullptr,
                              tr("Read tools config failed"),
                              tr("Can't open tools config file '%1' for read.")
                              .arg(filename));
        return;
    }

    QByteArray json = file.readAll().trimmed();
    if (json.isEmpty())
        return;
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(json,&error);
    if (error.error != QJsonParseError::NoError) {
        QMessageBox::critical(nullptr,
                              tr("Read tools config failed"),
                              tr("Read tools config file '%1' failed:%2")
                              .arg(filename,error.errorString()));
        return;
    }
    mTools.clear();
    QJsonArray array = doc.array();
    for(const QJsonValue& value: array) {
        QJsonObject object = value.toObject();
        PToolItem item = std::make_shared<ToolItem>();
        if (!object.contains("id"))
            item->id = QUuid::createUuid().toString();
        else
            item->id = object["id"].toString();
        item->title = object["title"].toString();
        if (item->title.isEmpty())
            continue;
        item->program = object["program"].toString();
        item->workingDirectory = object["workingDirectory"].toString();
        item->parameters = object["parameters"].toString();
        item->outputTarget = static_cast<ToolItemOutputTarget>(object["outputTarget"].toInt(0));
        item->inputOrigin= static_cast<ToolItemInputOrigin>(object["inputOrigin"].toInt(0));
        item->isUTF8 = object["isUTF8"].toBool(true);
        item->isBuiltIn = object["isBuiltIn"].toBool(false);
        // The title above was persisted in whatever language was active when the
        // file was written, so switching the language never updated it. Only the
        // built-in tool gets a fresh title in the current language; a user tool
        // keeps its own title, even if its command is the same as a built-in one.
        // Configurations written before the isBuiltIn field existed are migrated
        // when their title is one of the translations the built-in tool is saved
        // with (whatever language was active then), and its command is the built-in
        // one.
        if (!object.contains("isBuiltIn")
                && isKnownRemoveCompiledTitle(item->title)
                && isRemoveCompiledTool(item))
            item->isBuiltIn = true;
        if (item->isBuiltIn)
            item->title = tr("Remove Compiled");
        mTools.append(item);
    }
}

void ToolsManager::save()
{
    QString filename = includeTrailingPathDelimiter(pSettings->dirs().config()) + DEV_TOOLS_FILE;
    QFile file(filename);
    if (!file.open(QFile::WriteOnly | QFile::Truncate)) {
        QMessageBox::critical(nullptr,
                              tr("Save tools config failed"),
                              tr("Can't open tools config file '%1' for write.")
                              .arg(filename));
        return;
    }
    QJsonArray array;
    foreach (const PToolItem& tool,mTools) {
        QJsonObject object;
        object["id"]=tool->id;
        object["title"]=tool->title;
        object["program"]=tool->program;
        object["workingDirectory"] = tool->workingDirectory;
        object["parameters"]=tool->parameters;
        object["outputTarget"]=static_cast<int>(tool->outputTarget);
        object["inputOrigin"]=static_cast<int>(tool->inputOrigin);
        object["isUTF8"]=tool->isUTF8;
        object["isBuiltIn"]=tool->isBuiltIn;
        array.append(object);
    }
    QJsonDocument doc;
    doc.setArray(array);
    if (file.write(doc.toJson())<0) {
        QMessageBox::critical(nullptr,
                              tr("Save tools config failed"),
                              tr("Write to tools config file '%1' failed.")
                              .arg(filename));
        return;
    }

}

const QList<PToolItem> &ToolsManager::tools() const
{
    return mTools;
}

PToolItem ToolsManager::findTool(const QString &title)
{
    for (int i=0;i<mTools.count();i++) {
        PToolItem item = mTools[i];
        if (title == item->title) {
            return item;
        }
    }
    return PToolItem();
}

void ToolsManager::setTools(const QList<PToolItem> &newTools)
{
    mTools = newTools;
}
