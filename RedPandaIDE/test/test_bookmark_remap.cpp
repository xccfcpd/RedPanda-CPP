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
    model.moveBookmarksInFile(QStringLiteral("a.cpp"), lineMap, 5, false, false);
    QCOMPARE(bookmarkLines(model), QList<int>({2, 3}));
}

void TestBookmarkRemap::test_addBookmark_allowsSeveralOnTheSameLine()
{
    BookmarkModel model;
    model.addBookmark(QStringLiteral("a.cpp"), 1, QStringLiteral("first"), false);
    model.addBookmark(QStringLiteral("a.cpp"), 1, QStringLiteral("second"), false);
    QCOMPARE(model.rowCount(QModelIndex()), 2);
    QCOMPARE(model.bookmark(0, false)->description, QStringLiteral("first"));
    QCOMPARE(model.bookmark(1, false)->description, QStringLiteral("second"));
    QCOMPARE(model.bookmark(QStringLiteral("a.cpp"), 1, false)->description,
             QStringLiteral("first"));
}

void TestBookmarkRemap::test_move_stayPutBookmarksAreReserved()
{
    BookmarkModel model;
    model.addBookmark(QStringLiteral("a.cpp"), 0, QString(), false);
    model.addBookmark(QStringLiteral("a.cpp"), 1, QString(), false);
    // line 1 stays on its code, and line 0 is moved onto it: the two bookmarks
    // may share the merged line.
    QMap<int,int> lineMap{{0,1}};
    model.moveBookmarksInFile(QStringLiteral("a.cpp"), lineMap, 5, false, false);
    QCOMPARE(sortedBookmarkLines(model), QList<int>({1, 1}));
}

void TestBookmarkRemap::test_move_collisionPushedToTheNextFreeLine()
{
    BookmarkModel model;
    model.addBookmark(QStringLiteral("a.cpp"), 0, QString(), false);
    model.addBookmark(QStringLiteral("a.cpp"), 1, QString(), false);
    // both lines are reformatted into the same line 2; the two bookmarks may
    // share it.
    QMap<int,int> lineMap{{0,2}, {1,2}};
    model.moveBookmarksInFile(QStringLiteral("a.cpp"), lineMap, 5, false, false);
    QCOMPARE(sortedBookmarkLines(model), QList<int>({2, 2}));
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
    model.moveBookmarksInFile(QStringLiteral("a.cpp"), lineMap, lineCount, false, false);
    QList<int> lines = sortedBookmarkLines(model);
    QCOMPARE(lines, QList<int>({4, 4}));
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
    model.moveBookmarksInFile(QStringLiteral("a.cpp"), lineMap, 5, false, false);
    QCOMPARE(model.bookmark(QStringLiteral("b.cpp"), 0, false)->line, 0);
}

// The line the bookmark is set on is merged into the line above: the bookmark follows
// its code onto the merged line (the model is told right after the merge, and before
// the line is reported as deleted).
void TestBookmarkRemap::test_merge_movesTheBookmarkOfTheMergedLine()
{
    BookmarkModel model;
    model.addBookmark(QStringLiteral("a.cpp"), 1, QString(), false);
    model.onFileMergeLines(QStringLiteral("a.cpp"), 1, 0, 3, false);
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
    model.onFileMergeLines(QStringLiteral("a.cpp"), 1, 0, 3, false);
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
    model.onFileMergeLines(QStringLiteral("a.cpp"), 1, 0, 3, false);
    model.onFileDeleteLines(QStringLiteral("a.cpp"), 1, 1, false);
    // Several bookmarks may share the merged line now.
    QCOMPARE(sortedBookmarkLines(model), QList<int>({0, 0}));
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
    model.onFileMergeLines(QStringLiteral("a.cpp"), 5, 4, 6, false);
    model.onFileDeleteLines(QStringLiteral("a.cpp"), 5, 1, false);
    QCOMPARE(bookmarkLines(model), QList<int>({4}));
    // the two lines above the bookmark are removed: it moves up, away from the merge
    model.onFileDeleteLines(QStringLiteral("a.cpp"), 0, 2, false);
    QCOMPARE(bookmarkLines(model), QList<int>({2}));
    // a split is reported for the line the merge removed: it is not about that bookmark
    model.onFileSplitLines(QStringLiteral("a.cpp"), 4, 5, false);
    QCOMPARE(bookmarkLines(model), QList<int>({2}));
}

// The line map of a content change is keyed by the lines of the old content, and the
// bookmark of the line below a merge sits on the line the merge took away: looking the
// line of the bookmark that the split put back up in that map answers with the code of
// the bookmark below, and takes the first one away from its own line.
void TestBookmarkRemap::test_move_bookmarksPutBackByASplitAreReserved()
{
    BookmarkModel model;
    model.addBookmark(QStringLiteral("a.cpp"), 1, QString(), false);
    model.addBookmark(QStringLiteral("a.cpp"), 2, QString(), false);
    // the line 1 is joined onto the line 0, and the line 2 moves up on the line 1
    model.onFileMergeLines(QStringLiteral("a.cpp"), 1, 0, 3, false);
    model.onFileDeleteLines(QStringLiteral("a.cpp"), 1, 1, false);
    QCOMPARE(bookmarkLines(model), QList<int>({0, 1}));
    // undoing the merge splits the line back (inside the whole content change), and
    // the change ends by re-anchoring the bookmarks of the file - the model is told
    // that the content is restored by that undo (see Editor::isRestoringContentByUndoRedo())
    model.onFileSplitLines(QStringLiteral("a.cpp"), 0, 1, false);
    QMap<int,int> lineMap{{0,0}, {1,2}};
    model.moveBookmarksInFile(QStringLiteral("a.cpp"), lineMap, 4, false, true);
    QCOMPARE(sortedBookmarkLines(model), QList<int>({1, 2}));
}

// A merge that was never undone leaves its record behind. The whole content of the file
// can be replaced afterwards by something else than the undo of that merge (a reload from
// the disk, a reformat, the output of a tool): the record then describes the lines of a
// content that doesn't exist any more, and a split reported on the line of that old merge
// must not put the bookmark back on the line it was on in that old content.
void TestBookmarkRemap::test_split_recordsAreDroppedByAReplacementOfTheContent()
{
    BookmarkModel model;
    model.addBookmark(QStringLiteral("a.cpp"), 1, QString(), false);
    model.onFileMergeLines(QStringLiteral("a.cpp"), 1, 0, 3, false);
    model.onFileDeleteLines(QStringLiteral("a.cpp"), 1, 1, false);
    QCOMPARE(bookmarkLines(model), QList<int>({0}));
    // the new content is not the undo of that merge (the editor says so, see
    // Editor::isRestoringContentByUndoRedo()): the merge is forgotten
    model.moveBookmarksInFile(QStringLiteral("a.cpp"), QMap<int,int>(), 3, false, false);
    QCOMPARE(bookmarkLines(model), QList<int>({0}));
    // a split of the new content is reported on the line that old merge removed
    model.onFileSplitLines(QStringLiteral("a.cpp"), 0, 1, false);
    QCOMPARE(bookmarkLines(model), QList<int>({0}));
}

// The record is needed while the content is restored by an undo/redo: the split such a
// change runs reports the line a merge gave back, and the redo of a merge leaves a record
// that the next undo has to find. The whole content change of the redo must not drop it.
void TestBookmarkRemap::test_split_recordsAreKeptWhileTheContentIsRestoredByUndoRedo()
{
    BookmarkModel model;
    model.addBookmark(QStringLiteral("a.cpp"), 1, QString(), false);
    // the merge, then the undo of it: the split is reported inside the whole content
    // change of the undo, which re-anchors the bookmarks of the file right after
    model.onFileMergeLines(QStringLiteral("a.cpp"), 1, 0, 3, false);
    model.onFileDeleteLines(QStringLiteral("a.cpp"), 1, 1, false);
    model.onFileInsertLines(QStringLiteral("a.cpp"), 1, 1, false);
    model.onFileSplitLines(QStringLiteral("a.cpp"), 0, 1, false);
    QCOMPARE(bookmarkLines(model), QList<int>({1}));
    model.moveBookmarksInFile(QStringLiteral("a.cpp"), QMap<int,int>{{0,0}, {1,2}}, 3, false, true);
    QCOMPARE(bookmarkLines(model), QList<int>({1}));
    // the redo merges the lines again, and leaves a record behind: the next undo (the
    // split below) can only put the bookmark back if that record survived the redo
    model.onFileMergeLines(QStringLiteral("a.cpp"), 1, 0, 3, false);
    model.onFileDeleteLines(QStringLiteral("a.cpp"), 1, 1, false);
    QCOMPARE(bookmarkLines(model), QList<int>({0}));
    model.moveBookmarksInFile(QStringLiteral("a.cpp"), QMap<int,int>{{0,0}}, 2, false, true);
    model.onFileSplitLines(QStringLiteral("a.cpp"), 0, 1, false);
    QCOMPARE(bookmarkLines(model), QList<int>({1}));
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
                                       mEditor->content().count(), mEditor->inProject());
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
                                       mEditor->content().count(), mEditor->inProject());
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
    // Undoing an ordinary edit is reported line by line (see Editor::doUndo()): the
    // split it reports is what puts the bookmark back on its own line. The content
    // replacement callback is only used when the undo is the undo of a whole content
    // replacement (a formatting, a tool output), and must not be called here.
    mEditor->setContentReplacedFunc([this, &model](const QString& filename, bool inProject,
                                                   const QStringList& content,
                                                   const QMap<int,int>& markerLineMap) {
        model.moveBookmarksInFile(filename, markerLineMap, content.count(), inProject,
                                  mEditor->isRestoringContentByUndoRedo());
        mEditor->resetBookmarks(&model);
    });
    QVERIFY(mEditor->hasBookmark(1));
    connect(mEditor.get(), &QSynedit::QSynEdit::linesMerged, this,
            [this, &model](int removedLine, int intoLine) {
                model.onFileMergeLines(mEditor->filename(), removedLine, intoLine,
                                       mEditor->content().count(), mEditor->inProject());
                mEditor->resetBookmarks(&model);
            });
    connect(mEditor.get(), &QSynedit::QSynEdit::linesDeleted, this,
            [this, &model](int startLine, int count) {
                model.onFileDeleteLines(mEditor->filename(), startLine, count,
                                        mEditor->inProject());
                mEditor->resetBookmarks(&model);
            });
    // The insertion the undo of a merge runs (see doUndoItem()) is skipped while the
    // whole content is being replaced, like the application does (see
    // EditorManager::onEditorLinesInserted()): the bookmark the split put back on its
    // line is already on it, and the insertion must not move it off it again.
    connect(mEditor.get(), &QSynedit::QSynEdit::linesInserted, this,
            [this, &model](int startLine, int count) {
                if (mEditor->isReplacingContent())
                    return;
                model.onFileInsertLines(mEditor->filename(), startLine, count,
                                        mEditor->inProject());
                mEditor->resetBookmarks(&model);
            });
    // The undo is reported line by line, and the split is reported with it. The
    // application must not skip that report: it is the one that puts the bookmark back
    // on its line (see EditorManager::onEditorLinesSplit()).
    bool splitReported = false;
    connect(mEditor.get(), &QSynedit::QSynEdit::linesSplit, this,
            [this, &model, &splitReported](int mergedLine, int newLine) {
                splitReported = true;
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
    QVERIFY(splitReported);
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
                                       mEditor->content().count(), mEditor->inProject());
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

// Two bookmarks, one above the other: the line break between the lines they are on is
// deleted (the editor joins them), and undoing it gives the lines back. The bookmark of
// the joined line follows its code onto the line above, the one of the line below moves
// up while the join lasts - and both must be back on their own line. The undo is an
// ordinary edit: the insertion it reports moves the bookmark of the line below down
// again, and the split reported with it puts the bookmark of the joined line back on its
// own line.
void TestBookmarkRemap::test_editor_undoOfAJoinKeepsTheBookmarksOnTheirLines()
{
    const QString filename = QStringLiteral("a.cpp");
    mEditor->setFilename(filename);
    mEditor->setContent(QStringList({"int a;", "keepMe();", "int b;", "int c;"}));
    BookmarkModel model;
    model.addBookmark(filename, 1, QString(), false);
    model.addBookmark(filename, 2, QString(), false);
    mEditor->resetBookmarks(&model);
    mEditor->setContentReplacedFunc([this, &model](const QString &filename, bool inProject,
                                                   const QStringList &content,
                                                   const QMap<int,int> &markerLineMap) {
        model.moveBookmarksInFile(filename, markerLineMap, content.count(), inProject,
                                  mEditor->isRestoringContentByUndoRedo());
        mEditor->resetBookmarks(&model);
    });
    connect(mEditor.get(), &QSynedit::QSynEdit::linesMerged, this,
            [this, &model](int removedLine, int intoLine) {
                model.onFileMergeLines(mEditor->filename(), removedLine, intoLine,
                                       mEditor->content().count(), mEditor->inProject());
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
                if (mEditor->isReplacingContent())
                    return;
                model.onFileInsertLines(mEditor->filename(), startLine, count,
                                        mEditor->inProject());
                mEditor->resetBookmarks(&model);
            });
    connect(mEditor.get(), &QSynedit::QSynEdit::linesSplit, this,
            [this, &model](int mergedLine, int newLine) {
                model.onFileSplitLines(mEditor->filename(), mergedLine, newLine,
                                       mEditor->inProject());
                mEditor->resetBookmarks(&model);
            });
    // from the end of the first line to the beginning of the second one: the selection
    // holds the line break, so deleting it joins the two lines
    mEditor->setSelBeginEnd(QSynedit::CharPos{6, 0}, QSynedit::CharPos{0, 1});
    QTest::keyPress(mEditor.get(), Qt::Key_Delete);
    QCOMPARE(mEditor->content(), QStringList({"int a;keepMe();", "int b;", "int c;"}));
    QCOMPARE(sortedBookmarkLines(model), QList<int>({0, 1}));
    // the lines come back, and each bookmark is back on the line it was set on
    mEditor->undo();
    QCOMPARE(mEditor->content(), QStringList({"int a;", "keepMe();", "int b;", "int c;"}));
    QCOMPARE(sortedBookmarkLines(model), QList<int>({1, 2}));
    QVERIFY(mEditor->hasBookmark(1));
    QVERIFY(mEditor->hasBookmark(2));
    disconnect(mEditor.get(), nullptr, this, nullptr);
}

// The selection ends on the last line of the file: that line is joined to the first
// one, and the number of lines the models are told about must be the one the document
// has once the merge is done (see BookmarkModel::onFileMergeLines()). The bookmark of
// the merged line and the one of the last line share the merged line while the merge
// lasts (there is no line after it to put the second one on); undoing the merge splits
// the lines back, and each bookmark returns to its own line.
void TestBookmarkRemap::test_editor_deleteOfASelectionEndingOnTheLastLineKeepsTheBookmarks()
{
    const QString filename = QStringLiteral("a.cpp");
    mEditor->setFilename(filename);
    mEditor->setContent(QStringList({"aaaa", "bbbb", "cccc", "dddd"}));
    BookmarkModel model;
    model.addBookmark(filename, 1, QString(), false);
    model.addBookmark(filename, 3, QString(), false);
    mEditor->resetBookmarks(&model);
    connect(mEditor.get(), &QSynedit::QSynEdit::linesMerged, this,
            [this, &model](int removedLine, int intoLine) {
                model.onFileMergeLines(mEditor->filename(), removedLine, intoLine,
                                       mEditor->content().count(), mEditor->inProject());
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
                if (mEditor->isReplacingContent())
                    return;
                model.onFileInsertLines(mEditor->filename(), startLine, count,
                                        mEditor->inProject());
                mEditor->resetBookmarks(&model);
            });
    connect(mEditor.get(), &QSynedit::QSynEdit::linesSplit, this,
            [this, &model](int mergedLine, int newLine) {
                model.onFileSplitLines(mEditor->filename(), mergedLine, newLine,
                                       mEditor->inProject());
                mEditor->resetBookmarks(&model);
            });
    // from the second character of the second line to the beginning of the last one:
    // the last line keeps its text, and is joined to the first line
    mEditor->setSelBeginEnd(QSynedit::CharPos{2, 1}, QSynedit::CharPos{0, 3});
    QTest::keyPress(mEditor.get(), Qt::Key_Delete);
    QCOMPARE(mEditor->content(), QStringList({"aaaa", "bbdddd"}));
    // the two bookmarks are on the merged line for the time of the merge: there is no
    // line after it to push the second one to
    QCOMPARE(sortedBookmarkLines(model), QList<int>({1, 1}));
    // undoing the merge gives the lines back, and each bookmark is back on its own line
    mEditor->undo();
    QCOMPARE(mEditor->content(), QStringList({"aaaa", "bbbb", "cccc", "dddd"}));
    QCOMPARE(sortedBookmarkLines(model), QList<int>({1, 3}));
    QVERIFY(mEditor->hasBookmark(1));
    QVERIFY(mEditor->hasBookmark(3));
    disconnect(mEditor.get(), nullptr, this, nullptr);
}

// A line break is inserted at the very beginning of the document, above lines that hold
// the same text (here closing braces), and then undone: every line of the document
// moves down by one and back up again, and the bookmark has to follow it - the lines it
// passes through hold the same text as the one it is set on, and are surrounded by the
// same ones, so nothing in their content tells them apart.
void TestBookmarkRemap::test_editor_undoOfABreakAboveIdenticalLinesMovesTheBookmarkBack()
{
    const QString filename = QStringLiteral("a.cpp");
    mEditor->setFilename(filename);
    mEditor->setContent(QStringList({"}", "}", "}", "}"}));
    BookmarkModel model;
    model.addBookmark(filename, 3, QString(), false);
    mEditor->resetBookmarks(&model);
    QVERIFY(mEditor->hasBookmark(3));
    mEditor->setContentReplacedFunc([this, &model](const QString& filename, bool inProject,
                                                   const QStringList& content,
                                                   const QMap<int,int>& markerLineMap) {
        model.moveBookmarksInFile(filename, markerLineMap, content.count(), inProject,
                                  mEditor->isRestoringContentByUndoRedo());
        mEditor->resetBookmarks(&model);
    });
    // the line bookkeeping of the models is skipped while the whole content of the
    // editor is being replaced (see EditorManager::onEditorLinesInserted())
    connect(mEditor.get(), &QSynedit::QSynEdit::linesInserted, this,
            [this, &model](int startLine, int count) {
                if (mEditor->isReplacingContent())
                    return;
                model.onFileInsertLines(mEditor->filename(), startLine, count,
                                        mEditor->inProject());
                mEditor->resetBookmarks(&model);
            });
    connect(mEditor.get(), &QSynedit::QSynEdit::linesDeleted, this,
            [this, &model](int startLine, int count) {
                if (mEditor->isReplacingContent())
                    return;
                model.onFileDeleteLines(mEditor->filename(), startLine, count,
                                        mEditor->inProject());
                mEditor->resetBookmarks(&model);
            });
    mEditor->setCaretXY(QSynedit::CharPos{0, 0});
    QTest::keyPress(mEditor.get(), Qt::Key_Return);
    QCOMPARE(mEditor->content(), QStringList({"", "}", "}", "}", "}"}));
    QCOMPARE(bookmarkLines(model), QList<int>({4}));
    // the line goes away again: the bookmark is back on its own line
    mEditor->undo();
    QCOMPARE(mEditor->content(), QStringList({"}", "}", "}", "}"}));
    QCOMPARE(bookmarkLines(model), QList<int>({3}));
    QVERIFY(mEditor->hasBookmark(3));
    // redoing it puts that line back above the bookmark, which the line has to follow
    mEditor->redo();
    QCOMPARE(mEditor->content(), QStringList({"", "}", "}", "}", "}"}));
    QCOMPARE(bookmarkLines(model), QList<int>({4}));
    QVERIFY(mEditor->hasBookmark(4));
    disconnect(mEditor.get(), nullptr, this, nullptr);
}

// Every line of the file carries a bookmark, and the first two lines are joined: the
// bookmark of the joined line has no free line to be pushed to (see
// BookmarkModel::onFileMergeLines()), but it must not be left past the last line of the
// file either - a line no bookmark could ever be set on.
void TestBookmarkRemap::test_editor_mergeNeverPushesABookmarkPastTheLastLine()
{
    const QString filename = QStringLiteral("a.cpp");
    mEditor->setFilename(filename);
    mEditor->setContent(QStringList({"int a;", "int b;", "int c;"}));
    BookmarkModel model;
    model.addBookmark(filename, 0, QString(), false);
    model.addBookmark(filename, 1, QString(), false);
    model.addBookmark(filename, 2, QString(), false);
    mEditor->resetBookmarks(&model);
    connect(mEditor.get(), &QSynedit::QSynEdit::linesMerged, this,
            [this, &model](int removedLine, int intoLine) {
                model.onFileMergeLines(mEditor->filename(), removedLine, intoLine,
                                       mEditor->content().count(), mEditor->inProject());
                mEditor->resetBookmarks(&model);
            });
    connect(mEditor.get(), &QSynedit::QSynEdit::linesDeleted, this,
            [this, &model](int startLine, int count) {
                model.onFileDeleteLines(mEditor->filename(), startLine, count,
                                        mEditor->inProject());
                mEditor->resetBookmarks(&model);
            });
    // from the end of the first line to the beginning of the second one: the selection
    // holds the line break, so deleting it joins the two lines
    mEditor->setSelBeginEnd(QSynedit::CharPos{6, 0}, QSynedit::CharPos{0, 1});
    QTest::keyPress(mEditor.get(), Qt::Key_Delete);
    QCOMPARE(mEditor->content(), QStringList({"int a;int b;", "int c;"}));
    const int lineCount = mEditor->content().count();
    const QList<int> lines = bookmarkLines(model);
    QCOMPARE(lines.count(), 3);
    for (int line: lines) {
        QVERIFY2(line < lineCount, qPrintable(QStringLiteral("bookmark on line %1 of %2")
                                              .arg(line).arg(lineCount)));
    }
    disconnect(mEditor.get(), nullptr, this, nullptr);
}

// A bookmark is set on the line a merge goes into, and the merge is undone: the split
// that puts the merged bookmark back on its own line lands on the line of that other
// bookmark (see BookmarkModel::onFileSplitLines()). Two bookmarks of a file on the same
// line is what the model keeps out (see addBookmark()).
void TestBookmarkRemap::test_editor_undoOfAMergeNeverLeavesTwoBookmarksOnALine()
{
    const QString filename = QStringLiteral("a.cpp");
    mEditor->setFilename(filename);
    mEditor->setContent(QStringList({"int a;", "keepMe();", "int b;"}));
    BookmarkModel model;
    model.addBookmark(filename, 1, QString(), false);
    mEditor->resetBookmarks(&model);
    mEditor->setContentReplacedFunc([this, &model](const QString& filename, bool inProject,
                                                   const QStringList& content,
                                                   const QMap<int,int>& markerLineMap) {
        model.moveBookmarksInFile(filename, markerLineMap, content.count(), inProject,
                                  mEditor->isRestoringContentByUndoRedo());
        mEditor->resetBookmarks(&model);
    });
    connect(mEditor.get(), &QSynedit::QSynEdit::linesMerged, this,
            [this, &model](int removedLine, int intoLine) {
                model.onFileMergeLines(mEditor->filename(), removedLine, intoLine,
                                       mEditor->content().count(), mEditor->inProject());
                mEditor->resetBookmarks(&model);
            });
    connect(mEditor.get(), &QSynedit::QSynEdit::linesDeleted, this,
            [this, &model](int startLine, int count) {
                if (mEditor->isReplacingContent())
                    return;
                model.onFileDeleteLines(mEditor->filename(), startLine, count,
                                        mEditor->inProject());
                mEditor->resetBookmarks(&model);
            });
    connect(mEditor.get(), &QSynedit::QSynEdit::linesInserted, this,
            [this, &model](int startLine, int count) {
                if (mEditor->isReplacingContent())
                    return;
                model.onFileInsertLines(mEditor->filename(), startLine, count,
                                        mEditor->inProject());
                mEditor->resetBookmarks(&model);
            });
    connect(mEditor.get(), &QSynedit::QSynEdit::linesSplit, this,
            [this, &model](int mergedLine, int newLine) {
                model.onFileSplitLines(mEditor->filename(), mergedLine, newLine,
                                       mEditor->inProject());
                mEditor->resetBookmarks(&model);
            });
    // join the first two lines: the bookmark follows its code onto the joined line
    mEditor->setSelBeginEnd(QSynedit::CharPos{6, 0}, QSynedit::CharPos{0, 1});
    QTest::keyPress(mEditor.get(), Qt::Key_Delete);
    QCOMPARE(mEditor->content(), QStringList({"int a;keepMe();", "int b;"}));
    QCOMPARE(bookmarkLines(model), QList<int>({0}));
    // the user marks the line below while the merge lasts
    model.addBookmark(filename, 1, QString(), false);
    mEditor->resetBookmarks(&model);
    QCOMPARE(bookmarkLines(model), QList<int>({0, 1}));
    // undoing the merge puts the first bookmark back on its own line
    mEditor->undo();
    QCOMPARE(mEditor->content(), QStringList({"int a;", "keepMe();", "int b;"}));
    const QList<int> lines = bookmarkLines(model);
    QCOMPARE(lines.count(), 2);
    QVERIFY2(lines[0] != lines[1], qPrintable(QStringLiteral("two bookmarks on line %1")
                                              .arg(lines[0])));
    disconnect(mEditor.get(), nullptr, this, nullptr);
}

// A bookmark sits on a line a merge removes, and the user drops it while the merge
// lasts: what the merge remembered of it (see mMergedBookmarks) is about a bookmark the
// model does not hold any more, and undoing that merge must not put it back on its line.
void TestBookmarkRemap::test_split_doesNotPutBackABookmarkRemovedWhileTheMergeLasted()
{
    BookmarkModel model;
    model.addBookmark(QStringLiteral("a.cpp"), 1, QString(), false);
    model.onFileMergeLines(QStringLiteral("a.cpp"), 1, 0, 3, false);
    QCOMPARE(bookmarkLines(model), QList<int>({0}));
    // it is the bookmark the merge moved onto the merged line, and it is dropped here
    QVERIFY(model.removeBookmark(QStringLiteral("a.cpp"), 0, false));
    QCOMPARE(bookmarkLines(model), QList<int>());
    // the undo of the merge reports the line split back into it
    model.onFileSplitLines(QStringLiteral("a.cpp"), 0, 1, false);
    QCOMPARE(bookmarkLines(model), QList<int>());
}

// The project and the non-project models can hold a file with the same name. A content
// replacement of the non-project file must not drop what the project model remembered
// of its merge, or undoing that merge can't put its bookmark back.
void TestBookmarkRemap::test_move_doesNotDropTheMergeRecordOfTheOtherModel()
{
    const QString filename = QStringLiteral("a.cpp");
    BookmarkModel model;
    model.addBookmark(filename, 1, QString(), false);
    model.addBookmark(filename, 1, QString(), true);
    // a merge moves both bookmarks to line 0, each model keeps its own record
    model.onFileMergeLines(filename, 1, 0, 3, false);
    model.onFileDeleteLines(filename, 1, 1, false);
    model.onFileMergeLines(filename, 1, 0, 3, true);
    model.onFileDeleteLines(filename, 1, 1, true);
    QVERIFY(model.bookmark(filename, 0, false));
    QVERIFY(model.bookmark(filename, 0, true));
    // the non-project file gets its content replaced; only its own records go away
    model.moveBookmarksInFile(filename, QMap<int,int>(), 2, false, false);
    model.onFileSplitLines(filename, 0, 1, true);
    QVERIFY(model.bookmark(filename, 1, true));
    QVERIFY(model.bookmark(filename, 0, false));
}

// A file is renamed after a merge: the record of the merge must follow the new name,
// or the split the undo reports under that name can't find it.
void TestBookmarkRemap::test_renameMovesTheMergeRecord()
{
    const QString oldName = QStringLiteral("a.cpp");
    const QString newName = QStringLiteral("b.cpp");
    BookmarkModel model;
    model.addBookmark(oldName, 1, QString(), false);
    model.onFileMergeLines(oldName, 1, 0, 3, false);
    QCOMPARE(bookmarkLines(model), QList<int>({0}));
    model.renameBookmarkFile(oldName, newName, false);
    // the undo of the merge reports the split under the new name
    model.onFileSplitLines(newName, 0, 1, false);
    QCOMPARE(bookmarkLines(model), QList<int>({1}));
    QVERIFY(model.bookmark(newName, 1, false));
}

// A rename belongs to the file, not to one model: the bookmarks and the merge records
// of both the project and the non-project model follow it.
void TestBookmarkRemap::test_renameUpdatesBothModelsAndTheirSplitRecords()
{
    const QString oldName = QStringLiteral("a.cpp");
    const QString newName = QStringLiteral("b.cpp");
    BookmarkModel model;
    model.addBookmark(oldName, 1, QString(), false);
    model.addBookmark(oldName, 1, QString(), true);
    model.onFileMergeLines(oldName, 1, 0, 3, false);
    model.onFileMergeLines(oldName, 1, 0, 3, true);
    QVERIFY(model.bookmark(oldName, 0, false));
    QVERIFY(model.bookmark(oldName, 0, true));

    model.renameBookmarkFile(oldName, newName, false);

    QVERIFY(model.bookmark(newName, 0, false));
    QVERIFY(model.bookmark(newName, 0, true));
    QVERIFY(!model.bookmark(oldName, 0, false));
    QVERIFY(!model.bookmark(oldName, 0, true));
    // the split reports the new name, for both models
    model.onFileSplitLines(newName, 0, 1, false);
    model.onFileSplitLines(newName, 0, 1, true);
    QVERIFY(model.bookmark(newName, 1, false));
    QVERIFY(model.bookmark(newName, 1, true));
}

// Clearing one model must not drop the merge records of the other one: its
// bookmarks are still there and can still be put back by the split that undoes the
// merge.
void TestBookmarkRemap::test_clear_keepsTheMergeRecordOfTheOtherModel()
{
    const QString filename = QStringLiteral("a.cpp");
    BookmarkModel model;
    model.addBookmark(filename, 1, QString(), false);
    model.addBookmark(filename, 1, QString(), true);
    model.onFileMergeLines(filename, 1, 0, 3, false);
    model.onFileMergeLines(filename, 1, 0, 3, true);

    model.clear(false);
    QVERIFY(!model.bookmark(filename, 0, false));

    model.onFileSplitLines(filename, 0, 1, true);
    QVERIFY(model.bookmark(filename, 1, true));
}
