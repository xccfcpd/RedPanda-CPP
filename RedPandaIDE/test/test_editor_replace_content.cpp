#include "test_editor_replace_content.h"

#include "src/editor.h"

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
