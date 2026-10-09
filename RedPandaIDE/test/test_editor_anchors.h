#ifndef TEST_EDITOR_ANCHORS_H
#define TEST_EDITOR_ANCHORS_H
#include <QObject>

// The anchoring of the lines (see src/editoranchors.h) is what keeps the
// breakpoints, the bookmarks and the carets of a file on their code when the whole
// content is replaced. It works on plain text, so it is tested without a document.
class TestEditorAnchors : public QObject
{
    Q_OBJECT
public:
    explicit TestEditorAnchors(QObject *parent=nullptr);
private slots:
    // the key of a line
    void test_lineKey_keepsOnlyNonWhitespace();
    void test_lineKey_capsLongLines();
    void test_lineKey_capsAfterRemovingWhitespace();

    // the anchor of a line
    void test_lineAnchor_contextHoldsTheNeighbours();
    void test_lineAnchor_firstAndLastLine();
    void test_lineAnchor_matchesTheBreakpointAnchor();

    // the remapping of the lines
    void test_remap_contentUnchanged();
    void test_remap_linesInsertedBeforeTheAnchor();
    void test_remap_linesRemovedBeforeTheAnchor();
    void test_remap_reindentedContent();
    void test_remap_repeatedLinesUseTheirContext();
    void test_remap_anchorNotFoundKeepsTheOldLine();
    void test_remap_anchorPastTheEndOfTheContent();
    void test_remap_emptyContent();
    void test_remap_noAnchors();
};

#endif // TEST_EDITOR_ANCHORS_H
