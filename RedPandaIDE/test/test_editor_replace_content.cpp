#include "test_editor_replace_content.h"

#include "src/editor.h"
#include "src/debugger/breakpointanchor.h"

#include <QTest>
#include <qsynedit/qsynedit.h>

TestEditorReplaceContent::TestEditorReplaceContent(QObject *parent):
    TestEditorBase{parent}
{
}

void TestEditorReplaceContent::installRecorder()
{
    mCalls = 0;
    mLastContent.clear();
    mLastLineMap.clear();
    // "int main()" stands for a line a breakpoint, a bookmark or a caret is on
    mEditor->setAnchorLinesFunc([this]{ return QList<int>{1}; });
    mEditor->setContentReplacedFunc([this](const QString& /*filename*/, bool /*inProject*/,
                                           const QStringList& content,
                                           const QMap<int,int>& markerLineMap) {
        ++mCalls;
        mLastContent = content;
        mLastLineMap = markerLineMap;
    });
}

void TestEditorReplaceContent::init()
{
    installRecorder();
    mEditor->setContent(QStringList({"preamble","int main()","{","}"}));
    // the editor was told about the content above: the tests only watch the
    // replacement below
    mCalls = 0;
    mLastContent.clear();
    mLastLineMap.clear();
}

void TestEditorReplaceContent::test_replaceContent_notifiesTheModelsOnce()
{
    QVERIFY(!mEditor->isReplacingContent());
    mEditor->replaceContent(QStringLiteral("// a new line\npreamble\nint main()\n{\n}"));
    QCOMPARE(mCalls, 1);
    QVERIFY(!mEditor->isReplacingContent());
}

void TestEditorReplaceContent::test_replaceContent_forwardsTheNewContent()
{
    mEditor->replaceContent(QStringLiteral("// a new line\npreamble\nint main()\n{\n}"));
    QCOMPARE(mCalls, 1);
    QCOMPARE(mLastContent, QStringList({"// a new line","preamble","int main()","{","}"}));
}

void TestEditorReplaceContent::test_replaceContent_remapsTheAnchoredLines()
{
    // "int main()" was on line 1, and a line has been inserted before it
    mEditor->replaceContent(QStringLiteral("// a new line\npreamble\nint main()\n{\n}"));
    QCOMPARE(mCalls, 1);
    QCOMPARE(mLastLineMap.value(1), 2);
}

void TestEditorReplaceContent::test_replaceContent_keepsTheCaretOnItsCode()
{
    mEditor->setCaretXY(QSynedit::CharPos{3,1}); // inside "int main()"
    mEditor->replaceContent(QStringLiteral("// a new line\npreamble\nint main()\n{\n}"));
    QCOMPARE(mEditor->caretXY().line, 2);
}

void TestEditorReplaceContent::test_replaceContent_setsTheFlagWhileTheContentIsReplaced()
{
    // The models skip their incremental line bookkeeping exactly while the content
    // is replaced (EditorManager::onEditorLinesDeleted() returns early): the flag
    // must be up for every line signal of the replacement, and down once it is
    // over.
    bool sawLineSignal = false;
    bool flagUpForAllSignals = true;
    auto checkFlag = [this, &sawLineSignal, &flagUpForAllSignals](int, int) {
        sawLineSignal = true;
        if (!mEditor->isReplacingContent())
            flagUpForAllSignals = false;
    };
    connect(mEditor.get(), &QSynedit::QSynEdit::linesDeleted, this, checkFlag);
    connect(mEditor.get(), &QSynedit::QSynEdit::linesInserted, this, checkFlag);
    mEditor->replaceContent(QStringLiteral("// a new line\npreamble\nint main()\n{\n}"));
    // the lambda captures locals: drop the connections before leaving the test,
    // they would otherwise write into dead stack space on the next replacement
    disconnect(mEditor.get(), nullptr, this, nullptr);
    QVERIFY(sawLineSignal);
    QVERIFY(flagUpForAllSignals);
    QVERIFY(!mEditor->isReplacingContent());
}

void TestEditorReplaceContent::test_undoOfAWholeContentChange_notifiesTheModelsOnce()
{
    mEditor->replaceContent(QStringLiteral("// a new line\npreamble\nint main()\n{\n}"));
    QCOMPARE(mCalls, 1);
    mCalls = 0;
    mLastLineMap.clear();
    // Undoing a change that covered the whole document must go through the same
    // closed loop: the models are told once, and the flag comes back down.
    mEditor->undo();
    QCOMPARE(mCalls, 1);
    QCOMPARE(mEditor->content(), QStringList({"preamble","int main()","{","}"}));
    QVERIFY(!mEditor->isReplacingContent());
}

void TestEditorReplaceContent::test_redoOfAWholeContentChange_notifiesTheModelsOnce()
{
    mEditor->replaceContent(QStringLiteral("// a new line\npreamble\nint main()\n{\n}"));
    mEditor->undo();
    mCalls = 0;
    mLastLineMap.clear();
    mEditor->redo();
    QCOMPARE(mCalls, 1);
    QVERIFY(!mEditor->isReplacingContent());
}

// The report of a breakpoint that is off after astyle: the lines around the code it
// is set on were joined by an edit (the code is inside another line, and the model
// keeps the anchor that remembers it - see
// BreakpointModel::onFileMergeLines()), then the file was reformatted, which astyle
// does by replacing the whole content (see Editor::replaceContent()). The reformat
// splits the merged line back and rewrites what surrounds the code ("{} " in front of
// it, another tail), so the code is on a line of its own again without being that
// line: re-anchoring has to find it there.
void TestEditorReplaceContent::test_replaceContent_reanchorsTheBreakpointOfAMergedLineOnItsCode()
{
    QStringList initial({
        "#include <iostream>",
        "// #include <stdlib.h>",
        "// #include <math.h>",
        "using namespace std;",
        "int main() {",
        "    int i, j;",
        "    for (i = 1; i < 10; i++) {",
        "        for (j = 1; j <= i; j++) {{",
        "            cout << i << \"*\" << j << \"=\" << i*j << \" \";}}}",
        "            cout << \"\\n\";",
        "    }",
        "}"});
    mEditor->setContent(initial);
    // the model of the breakpoints, as far as this test is concerned: the line, and
    // the anchor of the code the breakpoint was set on
    int line = 5; // the merge joined the code onto the line of "int i, j;"
    QString anchor = contentContextFingerprint(initial, 8);
    mEditor->setContentReplacedFunc([this, &line, &anchor](const QString& /*filename*/,
                                                           bool /*inProject*/,
                                                           const QStringList& content,
                                                           const QMap<int,int>& /*markerLineMap*/) {
        // what BreakpointModel::reanchorBreakpoints() does for one breakpoint
        ReanchoredBreakpoint reanchored = reanchorBreakpoint(content, line, anchor);
        line = reanchored.line;
        anchor = reanchored.fingerprint;
    });
    QStringList formatted({
        "#include <iostream>",
        "// #include <stdlib.h>",
        "// #include <math.h>",
        "using namespace std;",
        "int main() {",
        "    int i, j;",
        "    for (i = 1; i < 10; i++) {",
        "        for (j = 1; j <= i; j++) {",
        "            {} cout << i << \"*\" << j << \"=\" << i*j << \" \",",
        "        }",
        "    }",
        "}",
        "} cout << \"\\n\";",
        "}",
        "}"});
    mEditor->replaceContent(formatted.join(QLatin1Char('\n')));
    QCOMPARE(line, 8);
    QCOMPARE(anchor, contentContextFingerprint(formatted, 8));
    // the lambda captures locals: put the recorder back before leaving the test
    installRecorder();
}

// The report itself, with the wiring of the application in place instead of a single
// callback: an edit joins the lines around the code (the code is then *inside* another
// line, and the model keeps the anchor that remembers it - see
// BreakpointModel::onFileMergeLines()), then the file is reformatted (astyle replaces
// the whole content - see Editor::replaceContent()). The line bookkeeping of the
// models is off while the content is replaced (the editor remaps the lines itself -
// see Editor::isReplacingContent()), but the merges and the splits the replacement
// makes are still reported, and EditorManager::onEditorLinesSplit() re-anchors the
// breakpoints on the content of the editor when one is. Re-anchoring on a content that
// is not in place yet (the replacement writes the document line by line, and it does
// so with an anchor that no longer holds the code: the line it was set on was merged,
// so what the editor reports as a line of the file at that moment is not what the file
// holds) is what leaves the breakpoint on the line the merge put it on, in the middle
// of other code, instead of the one holding its code.
void TestEditorReplaceContent::test_replaceContent_reanchorsTheBreakpointOfAMergedLineWithTheAppWiring()
{
    mEditor->setFilename(QStringLiteral("a.cpp"));
    QStringList initial({
        "#include <iostream>",
        "// #include <stdlib.h>",
        "// #include <math.h>",
        "using namespace std;",
        "int main() {",
        "    int i, j;",
        "    for (i = 1; i < 10; i++) {",
        "        for (j = 1; j <= i; j++) {",
        "            cout << i << \"*\" << j << \"=\" << i * j << \" \";",
        "        }",
        "            cout << \"\\n\";",
        "    }",
        "}"});
    mEditor->setContent(initial);
    // what BreakpointModel holds for the breakpoint of the file: the line it is on, and
    // the anchor of the code it was set on
    int line = 8;
    QString anchor = contentContextFingerprint(initial, 8);
    // what EditorManager::onEditorLinesSplit() does: re-anchor the breakpoints on the
    // content of the editor
    auto reanchorOnTheContentOfTheEditor = [this, &line, &anchor] {
        // what BreakpointModel::reanchorBreakpoints() does for one breakpoint
        ReanchoredBreakpoint reanchored = reanchorBreakpoint(mEditor->content(), line, anchor);
        line = reanchored.line;
        anchor = reanchored.fingerprint;
    };
    connect(mEditor.get(), &QSynedit::QSynEdit::linesSplit, this,
            [&reanchorOnTheContentOfTheEditor](int /*mergedLine*/, int /*newLine*/) {
                reanchorOnTheContentOfTheEditor();
            });
    // what BreakpointModel::onFileMergeLines() does with the line of the breakpoint
    connect(mEditor.get(), &QSynedit::QSynEdit::linesMerged, this,
            [&line](int removedLine, int intoLine) {
                if (line==removedLine)
                    line = intoLine;
                else if (line>removedLine)
                    --line;
            });
    // what EditorManager::onEditorContentReplaced() does
    mEditor->setContentReplacedFunc([&line, &anchor](const QString& /*filename*/,
                                                     bool /*inProject*/,
                                                     const QStringList& content,
                                                     const QMap<int,int>& /*markerLineMap*/) {
        ReanchoredBreakpoint reanchored = reanchorBreakpoint(content, line, anchor);
        line = reanchored.line;
        anchor = reanchored.fingerprint;
    });
    // the edit joins the three lines after "int i, j;" into it, one line break at a time
    for (int i=0;i<3;i++) {
        mEditor->setSelBeginEnd(QSynedit::CharPos{mEditor->content().at(5).length(),5},
                                QSynedit::CharPos{0,6});
        QTest::keyPress(mEditor.get(), Qt::Key_Delete);
    }
    QCOMPARE(mEditor->content().count(), 10);
    QCOMPARE(line, 5); // the code is inside the line of "int i, j;" now
    QVERIFY(!mEditor->isReplacingContent());
    // astyle runs: the merged line is split back and what surrounds the code is rewritten
    QStringList formatted({
        "#include <iostream>",
        "// #include <stdlib.h>",
        "// #include <math.h>",
        "using namespace std;",
        "int main() {",
        "    int i, j;",
        "    for (i = 1; i < 10; i++) {",
        "        for (j = 1; j <= i; j++) {",
        "            {} cout << i << \"*\" << j << \"=\" << i*j << \" \",",
        "        }",
        "    }",
        "}",
        "} cout << \"\\n\";",
        "}",
        "}"});
    mEditor->replaceContent(formatted.join(QLatin1Char('\n')));
    QCOMPARE(line, 8);
    QCOMPARE(anchor, contentContextFingerprint(formatted, 8));
    // the connections and the lambda capture locals: put them back before leaving
    disconnect(mEditor.get(), nullptr, this, nullptr);
    installRecorder();
}
