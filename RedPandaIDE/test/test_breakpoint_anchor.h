#ifndef TEST_BREAKPOINT_ANCHOR_H
#define TEST_BREAKPOINT_ANCHOR_H
#include <QObject>

// Re-anchoring a breakpoint on its code (see breakpointanchor.h): the line is
// found back by the text it holds and by the text of its neighbours, so that
// repeated lines can be told apart and a too-short line is never relied on.
class TestBreakpointAnchor : public QObject
{
    Q_OBJECT
public:
    explicit TestBreakpointAnchor(QObject *parent=nullptr);
private slots:
    void test_contextTellsRepeatedLinesApart();
    void test_nearestLineWins();
    void test_plainAnchorOfOneCharIsNeverUsed();
    void test_uniqueOnlyRejectsAnAmbiguousAnchor();
    void test_reanchor_followsTheCode();
    void test_reanchor_remembersTheAnchorOfAnOlderBreakpoint();
    void test_reanchor_keepsTheLineWhenTheCodeIsGone();
    void test_reanchor_doesNothingWhenNothingMoved();
    void test_reanchor_readsAPlainFingerprint();
    void test_reanchor_scalesToALargeContent();
};

#endif // TEST_BREAKPOINT_ANCHOR_H
