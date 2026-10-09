#include "test_bookmark_remap.h"

#include "src/widgets/bookmarkmodel.h"

#include <QTest>

#include <algorithm>

TestBookmarkRemap::TestBookmarkRemap(QObject *parent):
    QObject{parent}
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
