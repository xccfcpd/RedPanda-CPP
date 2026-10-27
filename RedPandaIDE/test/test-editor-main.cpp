#include <QTest>
#include <QGuiApplication>
#include <QFileInfo>
#include <QStringList>
#include "test_editor_symbol_completion.h"
#include "test_editor_anchors.h"
#include "test_editor_replace_content.h"
#include "test_bookmark_remap.h"
#include "test_formatter_arguments.h"
#include "test_breakpoint_anchor.h"
#include "test_breakpoint_remap.h"

// QTest overwrites its -o file for every suite when several suites are run from
// one program.  Give each suite its own file so a failing suite cannot hide
// behind the last one.
static int runSuite(QObject* suite, const QStringList& baseArgs)
{
    QStringList args = baseArgs;
    int outputIndex = args.indexOf(QStringLiteral("-o"));
    if (outputIndex>=0 && outputIndex+1<args.count()) {
        QStringList parts = args[outputIndex+1].split(QLatin1Char(','));
        if (!parts.isEmpty()) {
            QString path = parts[0];
            int dot = path.lastIndexOf(QLatin1Char('.'));
            QString suffix = QStringLiteral(".") + QString::fromLatin1(suite->metaObject()->className());
            if (dot>0)
                path.insert(dot, suffix);
            else
                path += suffix;
            parts[0] = path;
            args[outputIndex+1] = parts.join(QLatin1Char(','));
        }
    }
    return QTest::qExec(suite, args);
}

int main(int argc, char *argv[]) {
    int status = 0;
    QStringList baseArgs;
    for (int i=0;i<argc;i++)
        baseArgs.append(QString::fromLocal8Bit(argv[i]));
    QTest::setMainSourcePath(__FILE__, QT_TESTCASE_BUILDDIR); // Optional: for source path resolution

    QApplication app(argc,argv);
    //CharPos Test
    {
        TestEditorSymbolCompletion tc;
        status |= runSuite(&tc, baseArgs);
    }
    //the anchoring of the lines, used to keep the markers on their code
    {
        TestEditorAnchors tc;
        status |= runSuite(&tc, baseArgs);
    }
    //the whole content of a file is replaced: the models are told about it once
    {
        TestEditorReplaceContent tc;
        status |= runSuite(&tc, baseArgs);
    }
    //the bookmarks follow the line map of a content replacement
    {
        TestBookmarkRemap tc;
        status |= runSuite(&tc, baseArgs);
    }
    //the command line the formatters are run with, built from the settings
    {
        TestFormatterArguments tc;
        status |= runSuite(&tc, baseArgs);
    }
    //re-anchoring a breakpoint on its code when the file changes
    {
        TestBreakpointAnchor tc;
        status |= runSuite(&tc, baseArgs);
    }
    //several breakpoints on different lines of a file, through the real model
    {
        TestBreakpointRemap tc;
        status |= runSuite(&tc, baseArgs);
    }

    return status;
}
