#ifndef TEST_BOOKMARK_REMAP_H
#define TEST_BOOKMARK_REMAP_H
#include <QObject>

// BookmarkModel::moveBookmarksInFile() moves the bookmarks of a file to the lines
// of the new content (see Editor::isReplacingContent()). The model doesn't hold two
// bookmarks on the same line, so a colliding one is pushed to the next free line -
// but it must stay inside the document.
class TestBookmarkRemap : public QObject
{
    Q_OBJECT
public:
    explicit TestBookmarkRemap(QObject *parent=nullptr);
private slots:
    void test_move_usesTheLineMap();
    void test_move_stayPutBookmarksAreReserved();
    void test_move_collisionPushedToTheNextFreeLine();
    void test_move_neverPushedPastTheLastLine();
    void test_move_onlyTheGivenFileIsMoved();
};

#endif // TEST_BOOKMARK_REMAP_H
