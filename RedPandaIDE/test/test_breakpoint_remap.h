#ifndef TEST_BREAKPOINT_REMAP_H
#define TEST_BREAKPOINT_REMAP_H
#include "test_editor_base.h"

// Several breakpoints can be set on different lines of the same file. The real
// BreakpointModel is driven here the way EditorManager drives it: line signals for
// the ordinary edits and the undo of a merge, and reanchorBreakpoints() for a whole
// content replacement. The last test drives a real editor and the real model together.
class TestBreakpointRemap : public TestEditorBase
{
    Q_OBJECT
public:
    explicit TestBreakpointRemap(QObject *parent=nullptr);
private slots:
    void test_mergeAndUndoKeepsSeveralBreakpointsOnTheirLines();
    void test_undoOfALineDeletionRestoresItsBreakpoint();
    void test_plainInsertionDoesNotRestoreADeletedBreakpoint();
    void test_breakpointToggleUsesApplicationUndo();
    void test_addBreakpointOnAnEmptyLineSnapsToCode();
    void test_addBreakpointOnAnEmptyLineSkipsBracesAndComments();
    void test_addBreakpointOnAnEmptyLineSkipsElseAndAccessSpecifiers();
    void test_snapEmptyLineBreakpointMovesToCode();
    void test_addBreakpointOnAnEmptyLineSkipsCaseDefaultAndLabels();
    void test_addBreakpointOnAnEmptyLineSkipsLabelsWithComments();
    void test_addBreakpointOnAnEmptyLineAcceptsOneLineCase();
    void test_addBreakpointOnAnEmptyLineSkipsBlockCommentLabels();
    void test_addBreakpointOnAnEmptyLineSkipsMultilineBlockComment();
    void test_addBreakpointOnAnEmptyLineAcceptsOneLineCaseWithBlockComment();
    void test_addBreakpointOnAnEmptyLineKeepsSlashString();
    void test_addBreakpointOnAnEmptyLineKeepsRawString();
    void test_updateBreakpointNumberUsesCondition();
    void test_snapMigratesBothModelsAndKeepsDeletions();
    void test_snapOnlyNotifiesTheActiveModel();
    void test_snapKeepsOtherDeletedBreakpointHistory();
    void test_snapKeepsSeveralDeletedBreakpoints();
    void test_snapRemovesMigratedBreakpointFromItsOwnHistory();
    void test_snapKeepsDeletionHistoryForBothModels();
    void test_updateBreakpointNumberForSeveralBreakpointsOnALine();
    void test_lineSignalsKeepSeveralBreakpointsInStep();
    void test_reanchorAfterAReflowKeepsSeveralBreakpoints();
    void test_reanchorKeepsTheAnchorsOfSplitContinuationLines();
    void test_reanchorKeepsBothIndependentBreakpointsOnACollision();
    void test_editor_mergeAndUndoKeepsSeveralBreakpoints();
    void test_editor_reformatUndoKeepsSeveralBreakpoints();
    void test_editor_reformatKeepsBreakpointAndBookmark();
    void test_editor_realFormattersKeepBreakpointAndBookmark();
    void test_renameUpdatesBothModels();
};

#endif // TEST_BREAKPOINT_REMAP_H
