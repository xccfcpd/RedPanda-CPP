#include <QTest>
#include <QGuiApplication>
#include "test_editor_symbol_completion.h"
#include "test_editor_anchors.h"
#include "test_editor_replace_content.h"
#include "test_bookmark_remap.h"
#include "test_formatter_arguments.h"
#include "test_breakpoint_anchor.h"

int main(int argc, char *argv[]) {
    int status = 0;
    QTest::setMainSourcePath(__FILE__, QT_TESTCASE_BUILDDIR); // Optional: for source path resolution

    QApplication app(argc,argv);
    //CharPos Test
    {
        TestEditorSymbolCompletion tc;
        status |= QTest::qExec(&tc, argc, argv);
    }
    //the anchoring of the lines, used to keep the markers on their code
    {
        TestEditorAnchors tc;
        status |= QTest::qExec(&tc, argc, argv);
    }
    //the whole content of a file is replaced: the models are told about it once
    {
        TestEditorReplaceContent tc;
        status |= QTest::qExec(&tc, argc, argv);
    }
    //the bookmarks follow the line map of a content replacement
    {
        TestBookmarkRemap tc;
        status |= QTest::qExec(&tc, argc, argv);
    }
    //the command line the formatters are run with, built from the settings
    {
        TestFormatterArguments tc;
        status |= QTest::qExec(&tc, argc, argv);
    }
    //re-anchoring a breakpoint on its code when the file changes
    {
        TestBreakpointAnchor tc;
        status |= QTest::qExec(&tc, argc, argv);
    }

    return status;
}
