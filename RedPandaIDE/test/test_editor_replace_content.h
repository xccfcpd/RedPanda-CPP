#ifndef TEST_EDITOR_REPLACE_CONTENT_H
#define TEST_EDITOR_REPLACE_CONTENT_H
#include "test_editor_base.h"

#include <QMap>
#include <QStringList>

// Editor::replaceContent() and the undo/redo of such a change replace the whole
// content of the document. The markers of the file and the caret follow the code
// they are on, and the models are told about it exactly once (see
// Editor::isReplacingContent() and Editor::ContentReplacedFunc). These tests pin
// that closed loop down, with the models replaced by a recording callback.
class TestEditorReplaceContent : public TestEditorBase
{
    Q_OBJECT
public:
    explicit TestEditorReplaceContent(QObject *parent=nullptr);
private slots:
    void init();

    void test_replaceContent_notifiesTheModelsOnce();
    void test_replaceContent_forwardsTheNewContent();
    void test_replaceContent_remapsTheAnchoredLines();
    void test_replaceContent_keepsTheCaretOnItsCode();
    void test_replaceContent_setsTheFlagWhileTheContentIsReplaced();
    void test_undoOfAWholeContentChange_notifiesTheModelsOnce();
    void test_redoOfAWholeContentChange_notifiesTheModelsOnce();
    void test_replaceContent_reanchorsTheBreakpointOfAMergedLineOnItsCode();
    void test_replaceContent_reanchorsTheBreakpointOfAMergedLineWithTheAppWiring();
private:
    // Records what the editor tells the models (see ContentReplacedFunc) and
    // anchors one line, to stand for the breakpoints, the bookmarks and the carets
    // of the file (see GetAnchorLinesFunc).
    void installRecorder();
    int mCalls;
    QStringList mLastContent;
    QMap<int,int> mLastLineMap;
};

#endif // TEST_EDITOR_REPLACE_CONTENT_H
