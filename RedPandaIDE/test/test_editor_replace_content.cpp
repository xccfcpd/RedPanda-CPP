#include "test_editor_replace_content.h"

#include "src/caretlist.h"
#include "src/editor.h"
#include "src/debugger/breakpointanchor.h"
#include "src/settings/editorsettings.h"

#include <QTest>
#include <QFile>
#include <QTemporaryDir>
#include <QThread>
#include <qsynedit/qsynedit.h>

#include <algorithm>
#include <memory>

namespace {

// A formatter that takes long enough for a save to happen while it runs. It is
// not a real code formatter: it only makes the reformat recognizable.
class SlowReformatter : public BaseReformatter
{
public:
    QString reformat(const QString& content, QString& /*errorMessage*/, bool& isOk) override
    {
        QThread::msleep(100);
        isOk = true;
        return QStringLiteral("// formatted\n") + content;
    }
};

// A formatter that always fails, the way a missing or unusable clang-format/astyle
// does: the error message is set and no content is returned.
class FailingReformatter : public BaseReformatter
{
public:
    QString reformat(const QString& /*content*/, QString& errorMessage, bool& isOk) override
    {
        isOk = false;
        errorMessage = QStringLiteral("the formatter is not usable");
        return QString();
    }
};

} // namespace

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
        mEditor->setSelBeginEnd(QSynedit::CharPos{(int)mEditor->content().at(5).length(),5},
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

// The models are told about the replacement *before* the caret is moved to its new line.
// The line map is about the lines the markers were on before the replacement, while the
// caret the editor reports when it moves is recorded by the application
// (EditorManager::onEditorStatusChanged() -> CaretList::addCaret()), where the entry is
// already on its code: remapping the history after that move would move that entry a
// second time (see CaretList::remapLines()).
void TestEditorReplaceContent::test_replaceContent_movesTheCaretAfterTheModelsWereTold()
{
    mEditor->setCaretXY(QSynedit::CharPos{3,1}); // inside "int main()"
    int caretLineWhenTheModelsWereTold = -1;
    QStringList order;
    connect(mEditor.get(), &QSynedit::QSynEdit::statusChanged, this,
            [&order](QSynedit::StatusChanges changes) {
        if (changes.testFlag(QSynedit::StatusChange::CaretY))
            order.append(QStringLiteral("caret"));
    });
    mEditor->setContentReplacedFunc([this, &order, &caretLineWhenTheModelsWereTold](
                                        const QString& /*filename*/, bool /*inProject*/,
                                        const QStringList& content,
                                        const QMap<int,int>& lineMap) {
        ++mCalls;
        mLastContent = content;
        mLastLineMap = lineMap;
        caretLineWhenTheModelsWereTold = mEditor->caretXY().line;
        order.append(QStringLiteral("models"));
    });
    mEditor->replaceContent(QStringLiteral("// a new line\npreamble\nint main()\n{\n}"));
    // installRecorder() clears what the replacement recorded: read it before restoring it
    const int newLineOfTheAnchoredLine = mLastLineMap.value(1);
    const int lineOfTheCaret = mEditor->caretXY().line;
    // the lambda captures locals: drop the connection before leaving the test
    disconnect(mEditor.get(), nullptr, this, nullptr);
    installRecorder();
    // the anchored line of "int main()" moved from 1 to 2, and the caret with it
    QCOMPARE(newLineOfTheAnchoredLine, 2);
    QCOMPARE(lineOfTheCaret, 2);
    // ...but the models were told while the caret had not been moved to it yet: the move
    // the application records in the history is reported after them
    QVERIFY(caretLineWhenTheModelsWereTold != 2);
    QVERIFY(order.lastIndexOf(QStringLiteral("caret"))
            > order.indexOf(QStringLiteral("models")));
}

// Undoing a whole content change is the other way around, and that is what makes it safe:
// the caret is put back where it was while the change is applied (QSynEdit restores it),
// but the notification of that move - which is what the application records the history
// entry from (EditorManager::onEditorStatusChanged() -> CaretList::addCaret()) - is only
// emitted after the models have been told about the replacement, and after the window in
// which the content is replaced has closed (see Editor::isReplacingContent()). The entry
// therefore joins the history once the lines that were already in it have been moved by
// the map: it is already on its code and the map is not applied to it (see
// CaretList::remapLines()). This test pins that order down - a caret move notified while
// the content is replaced would be recorded before the remap, and moved a second time.
void TestEditorReplaceContent::test_undoOfAWholeContentChange_movesTheCaretAfterTheModelsWereTold()
{
    mEditor->setCaretXY(QSynedit::CharPos{3,1}); // inside "int main()"
    mEditor->replaceContent(QStringLiteral("// a new line\npreamble\nint main()\n{\n}"));
    QCOMPARE(mEditor->caretXY().line, 2);
    // the replacement above is what the undo below takes back: only it is watched
    mCalls = 0;
    mLastLineMap.clear();
    QStringList order;
    QList<bool> caretMovedWhileTheContentWasReplaced;
    connect(mEditor.get(), &QSynedit::QSynEdit::statusChanged, this,
            [this, &order, &caretMovedWhileTheContentWasReplaced](
                QSynedit::StatusChanges changes) {
        if (!changes.testFlag(QSynedit::StatusChange::CaretY))
            return;
        order.append(QStringLiteral("caret"));
        caretMovedWhileTheContentWasReplaced.append(mEditor->isReplacingContent());
    });
    int caretLineWhenTheModelsWereTold = -1;
    mEditor->setContentReplacedFunc([this, &order, &caretLineWhenTheModelsWereTold](
                                        const QString& /*filename*/, bool /*inProject*/,
                                        const QStringList& content,
                                        const QMap<int,int>& lineMap) {
        ++mCalls;
        mLastContent = content;
        mLastLineMap = lineMap;
        caretLineWhenTheModelsWereTold = mEditor->caretXY().line;
        order.append(QStringLiteral("models"));
    });
    mEditor->undo();
    const int lineOfTheCaret = mEditor->caretXY().line;
    // installRecorder() clears what the undo recorded: read it before restoring it
    const int calls = mCalls;
    const QStringList content = mLastContent;
    const QStringList recordedOrder = order;
    const int caretMovesInsideTheReplacement = caretMovedWhileTheContentWasReplaced.count(true);
    const int caretMovesOutsideOfTheReplacement = caretMovedWhileTheContentWasReplaced.count(false);
    // the lambda captures locals: drop the connection before leaving the test
    disconnect(mEditor.get(), nullptr, this, nullptr);
    installRecorder();
    // the undo put "int main()" - and the caret in it - back on line 1, and the models
    // were told about the replacement once
    QCOMPARE(calls, 1);
    QCOMPARE(content, QStringList({"preamble","int main()","{","}"}));
    QCOMPARE(lineOfTheCaret, 1);
    // the caret itself is already where the undo put it back when the models are told...
    QCOMPARE(caretLineWhenTheModelsWereTold, 1);
    // ...but the history entry is recorded from the notification of that move, and it
    // comes after them
    QCOMPARE(recordedOrder, QStringList({QStringLiteral("models"), QStringLiteral("caret")}));
    // and outside of the window in which the content is replaced: the application
    // records that move in the history after the map has been applied to it
    QCOMPARE(caretMovesInsideTheReplacement, 0);
    QCOMPARE(caretMovesOutsideOfTheReplacement, 1);
}

// Undoing an ordinary line insertion is reported line by line (the models move
// their markers from that report), not as a whole content replacement. The repeated
// block looks the same before and after the undo, so only the line report can put
// the marker back on its line.
void TestEditorReplaceContent::test_undoOfALineInsertionRemapsTheMarkerOfARepeatedBlock()
{
    int markerLine = 3;
    mEditor->setContent(QStringList({"A","A","A","A","A","A"}));
    // the line bookkeeping of the models (EditorManager::onEditorLinesInserted()/
    // ...Deleted()) for one marker
    connect(mEditor.get(), &QSynedit::QSynEdit::linesInserted, this,
            [&markerLine](int startLine, int count) {
                if (markerLine>=startLine)
                    markerLine+=count;
            });
    connect(mEditor.get(), &QSynedit::QSynEdit::linesDeleted, this,
            [&markerLine](int startLine, int count) {
                if (markerLine>=startLine+count)
                    markerLine-=count;
                else if (markerLine>=startLine)
                    markerLine=startLine;
            });
    mEditor->setCaretXY(QSynedit::CharPos{0,0});
    mEditor->processCommand(QSynedit::EditCommand::LineBreak);
    QCOMPARE(mEditor->content().count(), 7);
    QCOMPARE(markerLine, 4);
    mCalls = 0;
    mLastLineMap.clear();
    mEditor->undo();
    QCOMPARE(mEditor->content().count(), 6);
    // the undo is not a whole content change: it is reported line by line, and the
    // marker follows the line back even inside the repeated block
    QCOMPARE(mCalls, 0);
    QCOMPARE(markerLine, 3);
    disconnect(mEditor.get(), nullptr, this, nullptr);
    installRecorder();
}

// Several markers (the breakpoints of a file) in the same undo: each one follows the
// line reports, and none of them is left on a stale line.
void TestEditorReplaceContent::test_undoOfALineInsertionRemapsSeveralMarkers()
{
    QList<int> markers{1,4};
    mEditor->setContent(QStringList({"int a(){","return 0;","}","int b(){","return 0;","}"}));
    connect(mEditor.get(), &QSynedit::QSynEdit::linesInserted, this,
            [&markers](int startLine, int count) {
                for (int& line: markers)
                    if (line>=startLine)
                        line+=count;
            });
    connect(mEditor.get(), &QSynedit::QSynEdit::linesDeleted, this,
            [&markers](int startLine, int count) {
                for (int& line: markers) {
                    if (line>=startLine+count)
                        line-=count;
                    else if (line>=startLine)
                        line=startLine;
                }
            });
    mEditor->setCaretXY(QSynedit::CharPos{0,0});
    mEditor->processCommand(QSynedit::EditCommand::LineBreak);
    QCOMPARE(mEditor->content().count(), 7);
    QCOMPARE(markers, QList<int>({2,5}));
    mCalls = 0;
    mLastLineMap.clear();
    mEditor->undo();
    QCOMPARE(mEditor->content().count(), 6);
    QCOMPARE(mCalls, 0);
    QCOMPARE(markers, QList<int>({1,4}));
    disconnect(mEditor.get(), nullptr, this, nullptr);
    installRecorder();
}

// A save needs the formatted content before it writes the file. If the user
// started a reformat and saves before it finished, save() must not silently write
// the unformatted content and then let the abandoned result mark the editor
// modified: it formats synchronously and ignores the stale result.
void TestEditorReplaceContent::test_save_whileReformatting_savesTheFormattedContent()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString fileName = dir.filePath(QStringLiteral("race.cpp"));
    {
        QFile file(fileName);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("int  main() { return 0; }\n");
    }

    // The reformat was requested explicitly, so the save has to complete it even
    // when "format when saved" is off.
    mEditorSettings->setAutoFormatWhenSaved(false);
    mEditor->setFilename(fileName);
    mEditor->setFileType(FileType::CppSource, false);
    mEditor->loadFile(fileName, false);
    mEditor->setGetReformatterFunc([](Editor*) {
        return std::make_unique<SlowReformatter>();
    });

    // Start the asynchronous reformat and save before it can finish.
    mEditor->reformat(false, false, true);
    QVERIFY(mEditor->save(false, false));

    {
        QFile file(fileName);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QString savedContent = QString::fromUtf8(file.readAll());
        QVERIFY2(savedContent.startsWith(QStringLiteral("// formatted")),
                 qPrintable(savedContent));
    }

    // Let the abandoned worker finish; its stale result must change nothing.
    QTest::qWait(300);
    QVERIFY(!mEditor->modified());

    mEditorSettings->setAutoFormatWhenSaved(false);
}

// A save that formats the content must not block on a modal dialog when the formatter
// fails: the error is logged, and the unformatted content is still written.
void TestEditorReplaceContent::test_save_withAFailingFormatterDoesNotOpenADialog()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString fileName = dir.filePath(QStringLiteral("failing.cpp"));
    {
        QFile file(fileName);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("int  main() { return 0; }\n");
    }

    mEditorSettings->setAutoFormatWhenSaved(true);
    mEditorSettings->setRemoveTrailingSpacesWhenSaved(false);
    mEditor->setFilename(fileName);
    mEditor->setFileType(FileType::CppSource, false);
    mEditor->loadFile(fileName, false);
    mEditor->setGetReformatterFunc([](Editor*) {
        return std::make_unique<FailingReformatter>();
    });

    // the formatter fails: the save must neither hang on a modal dialog nor fail
    QVERIFY(mEditor->save(false, false));
    {
        QFile file(fileName);
        QVERIFY(file.open(QIODevice::ReadOnly));
        // the content is written as it was: the failed reformat changed nothing
        // (the line ending depends on the platform, hence the prefix comparison)
        QVERIFY(QString::fromUtf8(file.readAll())
                .startsWith(QStringLiteral("int  main() { return 0; }")));
    }

    mEditorSettings->setAutoFormatWhenSaved(false);
    mEditor->setGetReformatterFunc(nullptr);
}

// A new file saved as a non-C++ file must be handled by the type of the name it is
// saved as: the empty name/type it had must not make the editor run the C++ formatter
// on a .txt file.
void TestEditorReplaceContent::test_saveAs_doesNotFormatANonCppFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString fileName = dir.filePath(QStringLiteral("notes.txt"));

    mEditorSettings->setAutoFormatWhenSaved(true);
    mEditorSettings->setRemoveTrailingSpacesWhenSaved(false);
    mEditor->setContent(QStringLiteral("some  text\n"));
    int calls = 0;
    mEditor->setGetReformatterFunc([&calls](Editor*) {
        ++calls;
        return std::make_unique<SlowReformatter>();
    });

    QVERIFY(mEditor->saveAs(fileName, false));
    // the file type (and the name) is taken before the reformat: .txt is not a C++
    // file, so the formatter is not called at all
    QCOMPARE(calls, 0);
    {
        QFile file(fileName);
        QVERIFY(file.open(QIODevice::ReadOnly));
        // the content is written as it is (the line ending depends on the platform)
        QVERIFY(QString::fromUtf8(file.readAll())
                .startsWith(QStringLiteral("some  text")));
    }

    mEditorSettings->setAutoFormatWhenSaved(false);
    mEditor->setGetReformatterFunc(nullptr);
}

// The caret history follows a merge and its undo like the breakpoints and the
// bookmarks: the entries of the removed line are not dropped, they are moved onto
// the merged line and put back on their own line when the merge is undone.
void TestEditorReplaceContent::test_caretHistoryFollowsAMergeAndItsUndo()
{
    CaretList carets;
    carets.addCaret(mEditor.get(), 1, 4);
    carets.addCaret(mEditor.get(), 1, 0);
    carets.addCaret(mEditor.get(), 3, 2);
    // line 1 is merged into line 0, then the line is reported as removed
    carets.onLinesMerged(mEditor.get(), 1, 0);
    carets.onLinesDeleted(mEditor.get(), 1, 1);
    QList<int> lines = carets.caretLines(mEditor.get());
    std::sort(lines.begin(), lines.end());
    QCOMPARE(lines, QList<int>({0, 2}));

    // undo: the line is inserted back, then the split is reported
    carets.onLinesInserted(mEditor.get(), 1, 1);
    carets.onLinesSplit(mEditor.get(), 0, 1);
    lines = carets.caretLines(mEditor.get());
    std::sort(lines.begin(), lines.end());
    QCOMPARE(lines, QList<int>({1, 3}));
}

// A line can hold several breakpoints: addBreakpoint() adds one even when the line
// already has one, so each conditional breakpoint can be managed on its own.
void TestEditorReplaceContent::test_addBreakpointKeepsSeveralBreakpointsOnTheSameLine()
{
    int added = 0;
    connect(mEditor.get(), &Editor::breakpointAdded, this,
            [&added](const Editor*, int line, const QString&) {
                QCOMPARE(line, 1);
                ++added;
            });
    mEditor->addBreakpoint(1);
    mEditor->addBreakpoint(1);
    QCOMPARE(added, 2);
    QVERIFY(mEditor->hasBreakpoint(1));
    disconnect(mEditor.get(), nullptr, this, nullptr);
}
