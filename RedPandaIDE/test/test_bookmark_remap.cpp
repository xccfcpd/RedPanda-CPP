#include "test_bookmark_remap.h"

#include "src/editor.h"
#include "src/widgets/bookmarkmodel.h"

#include <QTest>

#include <qsynedit/qsynedit.h>

#include <algorithm>

TestBookmarkRemap::TestBookmarkRemap(QObject *parent):
    TestEditorBase{parent}
{
}

// The lines of the bookmarks of the model, in the order of the model.
static QList<int> bookmarkLines(BookmarkModel& model)
{
    QList<int> lines;
    for (int i=0;i<model.rowCount(QModelIndex());i++)
        lines.append(model.bookmark(i)->line);
    return lines;
}

// The same, sorted: the order of the list is an implementation detail of the
// pushing below, only the set of lines matters to the callers.
static QList<int> sortedBookmarkLines(BookmarkModel& model)
{
    QList<int> lines = bookmarkLines(model);
    std::sort(lines.begin(), lines.end());
    return lines;
}

void TestBookmarkRemap::test_move_usesTheLineMap()
{
    BookmarkModel model;
    model.addBookmark(QStringLiteral("a.cpp"), 0, QString(), false);
    model.addBookmark(QStringLiteral("a.cpp"), 1, QString(), false);
    // the two lines were pushed down by two lines of header
    QMap<int,int> lineMap{{0,2}, {1,3}};
    model.moveBookmarksInFile(QStringLiteral("a.cpp"), lineMap, 5, false);
    QCOMPARE(bookmarkLines(model), QList<int>({2, 3}));
}

void TestBookmarkRemap::test_move_stayPutBookmarksAreReserved()
{
    BookmarkModel model;
    model.addBookmark(QStringLiteral("a.cpp"), 0, QString(), false);
    model.addBookmark(QStringLiteral("a.cpp"), 1, QString(), false);
    // line 1 stays on its code, and line 0 is moved onto it: the bookmark that
    // didn't move keeps its line, the other one is pushed to the next free one
    QMap<int,int> lineMap{{0,1}};
    model.moveBookmarksInFile(QStringLiteral("a.cpp"), lineMap, 5, false);
    QCOMPARE(sortedBookmarkLines(model), QList<int>({1, 2}));
}

void TestBookmarkRemap::test_move_collisionPushedToTheNextFreeLine()
{
    BookmarkModel model;
    model.addBookmark(QStringLiteral("a.cpp"), 0, QString(), false);
    model.addBookmark(QStringLiteral("a.cpp"), 1, QString(), false);
    // both lines are reformatted into the same line 2
    QMap<int,int> lineMap{{0,2}, {1,2}};
    model.moveBookmarksInFile(QStringLiteral("a.cpp"), lineMap, 5, false);
    QCOMPARE(sortedBookmarkLines(model), QList<int>({2, 3}));
}

void TestBookmarkRemap::test_move_neverPushedPastTheLastLine()
{
    // The regression: the last line is taken, and the document has no line after
    // it, so the colliding bookmark may not be pushed past the end (it used to be,
    // the push had no bound).
    BookmarkModel model;
    model.addBookmark(QStringLiteral("a.cpp"), 0, QString(), false);
    model.addBookmark(QStringLiteral("a.cpp"), 1, QString(), false);
    QMap<int,int> lineMap{{0,4}, {1,4}};
    const int lineCount = 5; // lines 0..4
    model.moveBookmarksInFile(QStringLiteral("a.cpp"), lineMap, lineCount, false);
    QList<int> lines = sortedBookmarkLines(model);
    QCOMPARE(lines, QList<int>({3, 4}));
    // no bookmark outside of the document
    foreach (int line, lines)
        QVERIFY(line>=0 && line<lineCount);
}

void TestBookmarkRemap::test_move_onlyTheGivenFileIsMoved()
{
    BookmarkModel model;
    model.addBookmark(QStringLiteral("a.cpp"), 0, QString(), false);
    model.addBookmark(QStringLiteral("b.cpp"), 0, QString(), false);
    QMap<int,int> lineMap{{0,2}};
    model.moveBookmarksInFile(QStringLiteral("a.cpp"), lineMap, 5, false);
    QCOMPARE(model.bookmark(QStringLiteral("b.cpp"), 0, false)->line, 0);
}

// The line the bookmark is set on is merged into the line above: the bookmark follows
// its code onto the merged line (the model is told right after the merge, and before
// the line is reported as deleted).
void TestBookmarkRemap::test_merge_movesTheBookmarkOfTheMergedLine()
{
    BookmarkModel model;
    model.addBookmark(QStringLiteral("a.cpp"), 1, QString(), false);
    model.onFileMergeLines(QStringLiteral("a.cpp"), 1, 0, false);
    model.onFileDeleteLines(QStringLiteral("a.cpp"), 1, 1, false);
    QCOMPARE(bookmarkLines(model), QList<int>({0}));
}

// Undoing that merge (Ctrl+Z) gives the line back: a bookmark has no fingerprint to
// find its code with, like a breakpoint does - the merge is the only thing that
// remembered that it was on that line.
void TestBookmarkRemap::test_split_putsTheBookmarkOfTheMergeBackOnItsLine()
{
    BookmarkModel model;
    model.addBookmark(QStringLiteral("a.cpp"), 1, QString(), false);
    model.onFileMergeLines(QStringLiteral("a.cpp"), 1, 0, false);
    model.onFileDeleteLines(QStringLiteral("a.cpp"), 1, 1, false);
    QCOMPARE(bookmarkLines(model), QList<int>({0}));
    // the undo puts the line back, then reports the split
    model.onFileInsertLines(QStringLiteral("a.cpp"), 1, 1, false);
    model.onFileSplitLines(QStringLiteral("a.cpp"), 0, 1, false);
    QCOMPARE(bookmarkLines(model), QList<int>({1}));
}

// A line split is reported for a line that wasn't merged by us (the user typed a
// return, and undid it): the bookmarks must stay where they are.
void TestBookmarkRemap::test_split_withoutAMergeDoesNothing()
{
    BookmarkModel model;
    model.addBookmark(QStringLiteral("a.cpp"), 1, QString(), false);
    model.onFileSplitLines(QStringLiteral("a.cpp"), 0, 1, false);
    QCOMPARE(bookmarkLines(model), QList<int>({1}));
}

// The merged line already has a bookmark: it follows its own code, which is at the
// beginning of the merged line, and the one that comes from the line below - whose own
// line is gone - takes the next one. The model holds no two bookmarks on a line, and
// undoing the merge leaves both on the lines the user set them on.
void TestBookmarkRemap::test_merge_pushesTheBookmarkWhenTheMergedLineAlreadyHasOne()
{
    BookmarkModel model;
    model.addBookmark(QStringLiteral("a.cpp"), 0, QString(), false);
    model.addBookmark(QStringLiteral("a.cpp"), 1, QString(), false);
    model.onFileMergeLines(QStringLiteral("a.cpp"), 1, 0, false);
    model.onFileDeleteLines(QStringLiteral("a.cpp"), 1, 1, false);
    QCOMPARE(sortedBookmarkLines(model), QList<int>({0, 1}));
    model.onFileInsertLines(QStringLiteral("a.cpp"), 1, 1, false);
    model.onFileSplitLines(QStringLiteral("a.cpp"), 0, 1, false);
    QCOMPARE(sortedBookmarkLines(model), QList<int>({0, 1}));
}

// A merge that was never undone leaves its record behind. A later, unrelated split on
// the same line must not move a bookmark that is not on that line any more: the
// bookmark only follows the merge while it is still on (or right below) the merged
// line - otherwise the edits in between took it away from it, and putting it back on
// the line of a merge nobody undid would be wrong.
void TestBookmarkRemap::test_split_leavesABookmarkThatIsNotOnTheMergedLineAlone()
{
    BookmarkModel model;
    model.addBookmark(QStringLiteral("a.cpp"), 5, QString(), false);
    model.onFileMergeLines(QStringLiteral("a.cpp"), 5, 4, false);
    model.onFileDeleteLines(QStringLiteral("a.cpp"), 5, 1, false);
    QCOMPARE(bookmarkLines(model), QList<int>({4}));
    // the two lines above the bookmark are removed: it moves up, away from the merge
    model.onFileDeleteLines(QStringLiteral("a.cpp"), 0, 2, false);
    QCOMPARE(bookmarkLines(model), QList<int>({2}));
    // a split is reported for the line the merge removed: it is not about that bookmark
    model.onFileSplitLines(QStringLiteral("a.cpp"), 4, 5, false);
    QCOMPARE(bookmarkLines(model), QList<int>({2}));
}

// The whole chain, on a real editor: the caret is at the end of the line above the
// bookmark and Delete joins the two lines - the bookmark's own line disappears. The
// handler is called with exactly what the editor's signals carry, the way
// EditorManager wires it, and the bookmark must end up on the merged line and still be
// shown by the editor.
void TestBookmarkRemap::test_editor_mergeOfTheBookmarkedLineKeepsTheBookmarkOnTheMergedLine()
{
    const QString filename = QStringLiteral("a.cpp");
    mEditor->setFilename(filename);
    mEditor->setContent(QStringList({"int a;", "keepMe();", "int b;"}));
    BookmarkModel model;
    model.addBookmark(filename, 1, QString(), false);
    mEditor->resetBookmarks(&model);
    QVERIFY(mEditor->hasBookmark(1));
    // what EditorManager does with the line signals of the editor
    connect(mEditor.get(), &QSynedit::QSynEdit::linesMerged, this,
            [this, &model](int removedLine, int intoLine) {
                model.onFileMergeLines(mEditor->filename(), removedLine, intoLine,
                                       mEditor->inProject());
                mEditor->resetBookmarks(&model);
            });
    connect(mEditor.get(), &QSynedit::QSynEdit::linesDeleted, this,
            [this, &model](int startLine, int count) {
                model.onFileDeleteLines(mEditor->filename(), startLine, count,
                                        mEditor->inProject());
                mEditor->resetBookmarks(&model);
            });
    mEditor->setCaretXY(QSynedit::CharPos{6, 0});
    QTest::keyPress(mEditor.get(), Qt::Key_Delete);
    QCOMPARE(mEditor->content(), QStringList({"int a;keepMe();", "int b;"}));
    // the bookmark followed its code onto the merged line, and the editor shows it there
    QCOMPARE(bookmarkLines(model), QList<int>({0}));
    QVERIFY(mEditor->hasBookmark(0));
    disconnect(mEditor.get(), nullptr, this, nullptr);
}

// The other direction: the caret is at the end of the line the bookmark is on, and the
// line below is joined to it. The bookmark's code did not move, so the bookmark must
// not move either - and the line that was removed must not take it away.
void TestBookmarkRemap::test_editor_mergeOfTheLineBelowKeepsTheBookmarkWhereItIs()
{
    const QString filename = QStringLiteral("a.cpp");
    mEditor->setFilename(filename);
    mEditor->setContent(QStringList({"int a;", "keepMe();", "int b;"}));
    BookmarkModel model;
    model.addBookmark(filename, 1, QString(), false);
    mEditor->resetBookmarks(&model);
    QVERIFY(mEditor->hasBookmark(1));
    connect(mEditor.get(), &QSynedit::QSynEdit::linesMerged, this,
            [this, &model](int removedLine, int intoLine) {
                model.onFileMergeLines(mEditor->filename(), removedLine, intoLine,
                                       mEditor->inProject());
                mEditor->resetBookmarks(&model);
            });
    connect(mEditor.get(), &QSynedit::QSynEdit::linesDeleted, this,
            [this, &model](int startLine, int count) {
                model.onFileDeleteLines(mEditor->filename(), startLine, count,
                                        mEditor->inProject());
                mEditor->resetBookmarks(&model);
            });
    mEditor->setCaretXY(QSynedit::CharPos{9, 1});
    QTest::keyPress(mEditor.get(), Qt::Key_Delete);
    QCOMPARE(mEditor->content(), QStringList({"int a;", "keepMe();int b;"}));
    QCOMPARE(bookmarkLines(model), QList<int>({1}));
    QVERIFY(mEditor->hasBookmark(1));
    disconnect(mEditor.get(), nullptr, this, nullptr);
}

// The same join, made the way it is usually made: the line break is selected (here with
// the whole next line) and deleted. The editor joins the lines itself, and the line the
// bookmark is on keeps its text - it is put back at the end of the line above, so the
// bookmark has to follow its code there. Reporting that line as deleted takes the
// bookmark away from code that is still in the document.
void TestBookmarkRemap::test_editor_deleteOfALineBreakKeepsTheBookmarkOfTheJoinedLine()
{
    const QString filename = QStringLiteral("a.cpp");
    mEditor->setFilename(filename);
    mEditor->setContent(QStringList({"int a;", "keepMe();", "int b;"}));
    BookmarkModel model;
    model.addBookmark(filename, 1, QString(), false);
    mEditor->resetBookmarks(&model);
    // The application runs undo through runWholeContentChange() (see Editor::doUndo())
    // and re-anchors the markers of the file afterwards (see ContentReplacedFunc): as
    // far as the models are concerned, the whole content of the document is replaced
    // while the undo runs (see Editor::isReplacingContent()).
    mEditor->setContentReplacedFunc([this, &model](const QString& filename, bool inProject,
                                                   const QStringList& content,
                                                   const QMap<int,int>& markerLineMap) {
        model.moveBookmarksInFile(filename, markerLineMap, content.count(), inProject);
        mEditor->resetBookmarks(&model);
    });
    QVERIFY(mEditor->hasBookmark(1));
    connect(mEditor.get(), &QSynedit::QSynEdit::linesMerged, this,
            [this, &model](int removedLine, int intoLine) {
                model.onFileMergeLines(mEditor->filename(), removedLine, intoLine,
                                       mEditor->inProject());
                mEditor->resetBookmarks(&model);
            });
    connect(mEditor.get(), &QSynedit::QSynEdit::linesDeleted, this,
            [this, &model](int startLine, int count) {
                model.onFileDeleteLines(mEditor->filename(), startLine, count,
                                        mEditor->inProject());
                mEditor->resetBookmarks(&model);
            });
    // Undoing runs as a whole content change (see Editor::doUndo()), and the split is
    // reported inside it. The application must not skip that report: it is the one that
    // puts the bookmark back on its line (see EditorManager::onEditorLinesSplit()).
    bool splitWhileReplacingContent = false;
    connect(mEditor.get(), &QSynedit::QSynEdit::linesSplit, this,
            [this, &model, &splitWhileReplacingContent](int mergedLine, int newLine) {
                splitWhileReplacingContent = splitWhileReplacingContent
                        || mEditor->isReplacingContent();
                model.onFileSplitLines(mEditor->filename(), mergedLine, newLine,
                                       mEditor->inProject());
                mEditor->resetBookmarks(&model);
            });
    // from the end of the first line to the beginning of the second one: the selection
    // holds the line break, so deleting it joins the two lines
    mEditor->setSelBeginEnd(QSynedit::CharPos{6, 0}, QSynedit::CharPos{0, 1});
    QTest::keyPress(mEditor.get(), Qt::Key_Delete);
    QCOMPARE(mEditor->content(), QStringList({"int a;keepMe();", "int b;"}));
    QCOMPARE(bookmarkLines(model), QList<int>({0}));
    QVERIFY(mEditor->hasBookmark(0));
    QVERIFY(!mEditor->isReplacingContent());
    // undoing that deletion splits the lines back: the bookmark follows its code again
    mEditor->undo();
    QCOMPARE(mEditor->content(), QStringList({"int a;", "keepMe();", "int b;"}));
    QVERIFY(splitWhileReplacingContent);
    QCOMPARE(bookmarkLines(model), QList<int>({1}));
    QVERIFY(mEditor->hasBookmark(1));
    disconnect(mEditor.get(), nullptr, this, nullptr);
}

// The usual way the lines get joined while editing: a selection that spans lines is
// typed over (the text of the selection is deleted first - see doSetSelTextPrimitive() -
// and that deletion joins the lines). Undoing it puts the lines back, and the bookmark
// must be back on the line it was set on.
void TestBookmarkRemap::test_editor_undoOfTypingOverASelectionPutsTheBookmarkBack()
{
    const QString filename = QStringLiteral("a.cpp");
    mEditor->setFilename(filename);
    mEditor->setContent(QStringList({"int a;", "keepMe();", "int b;", "int c;"}));
    BookmarkModel model;
    model.addBookmark(filename, 1, QString(), false);
    mEditor->resetBookmarks(&model);
    QVERIFY(mEditor->hasBookmark(1));
    connect(mEditor.get(), &QSynedit::QSynEdit::linesMerged, this,
            [this, &model](int removedLine, int intoLine) {
                model.onFileMergeLines(mEditor->filename(), removedLine, intoLine,
                                       mEditor->inProject());
                mEditor->resetBookmarks(&model);
            });
    connect(mEditor.get(), &QSynedit::QSynEdit::linesSplit, this,
            [this, &model](int mergedLine, int newLine) {
                model.onFileSplitLines(mEditor->filename(), mergedLine, newLine,
                                       mEditor->inProject());
                mEditor->resetBookmarks(&model);
            });
    connect(mEditor.get(), &QSynedit::QSynEdit::linesDeleted, this,
            [this, &model](int startLine, int count) {
                model.onFileDeleteLines(mEditor->filename(), startLine, count,
                                        mEditor->inProject());
                mEditor->resetBookmarks(&model);
            });
    connect(mEditor.get(), &QSynedit::QSynEdit::linesInserted, this,
            [this, &model](int startLine, int count) {
                model.onFileInsertLines(mEditor->filename(), startLine, count,
                                        mEditor->inProject());
                mEditor->resetBookmarks(&model);
            });
    // the code of the bookmarked line is part of the selection, so the typing joins it
    // onto the line above
    mEditor->setSelBeginEnd(QSynedit::CharPos{6, 0}, QSynedit::CharPos{6, 1});
    QTest::keyClicks(mEditor.get(), QStringLiteral("X"));
    QCOMPARE(mEditor->content(), QStringList({"int a;X();", "int b;", "int c;"}));
    QCOMPARE(bookmarkLines(model), QList<int>({0}));
    QVERIFY(mEditor->hasBookmark(0));
    // the lines come back: the bookmark must be back on its own line
    mEditor->undo();
    if (mEditor->content() != QStringList({"int a;", "keepMe();", "int b;", "int c;"}))
        mEditor->undo();
    QCOMPARE(mEditor->content(), QStringList({"int a;", "keepMe();", "int b;", "int c;"}));
    QCOMPARE(bookmarkLines(model), QList<int>({1}));
    QVERIFY(mEditor->hasBookmark(1));
    disconnect(mEditor.get(), nullptr, this, nullptr);
}
