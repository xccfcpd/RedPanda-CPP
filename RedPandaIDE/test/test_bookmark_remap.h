#ifndef TEST_BOOKMARK_REMAP_H
#define TEST_BOOKMARK_REMAP_H
#include "test_editor_base.h"

// BookmarkModel::moveBookmarksInFile() moves the bookmarks of a file to the lines
// of the new content (see Editor::isReplacingContent()). The model doesn't hold two
// bookmarks on the same line, so a colliding one is pushed to the next free line -
// but it must stay inside the document. The last test drives a real editor, to check
// the signals it sends on a merge against what the model expects from them.
class TestBookmarkRemap : public TestEditorBase
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
    void test_merge_movesTheBookmarkOfTheMergedLine();
    void test_split_putsTheBookmarkOfTheMergeBackOnItsLine();
    void test_split_withoutAMergeDoesNothing();
    void test_merge_pushesTheBookmarkWhenTheMergedLineAlreadyHasOne();
    void test_split_leavesABookmarkThatIsNotOnTheMergedLineAlone();
    void test_editor_mergeOfTheBookmarkedLineKeepsTheBookmarkOnTheMergedLine();
    void test_editor_mergeOfTheLineBelowKeepsTheBookmarkWhereItIs();
    void test_editor_deleteOfALineBreakKeepsTheBookmarkOfTheJoinedLine();
    void test_editor_undoOfTypingOverASelectionPutsTheBookmarkBack();
};

#endif // TEST_BOOKMARK_REMAP_H
