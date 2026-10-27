#include "test_breakpoint_remap.h"

#include "src/debugger/breakpointanchor.h"
#include "src/debugger/debuggermodels.h"
#include "src/editor.h"
#include "src/reformatter/astyleformatter.h"
#include "src/reformatter/basereformatter.h"
#include "src/reformatter/clangformatreformatter.h"
#include "src/widgets/bookmarkmodel.h"

#include <QDir>
#include <QFileInfo>
#include <QTest>
#include <QSignalSpy>

#include <memory>
#include <utility>

#include <qsynedit/qsynedit.h>

namespace {

PBreakpoint addBreakpoint(BreakpointModel& model, const QString& filename,
                          const QStringList& content, int line, bool forProject = false)
{
    PBreakpoint breakpoint = std::make_shared<Breakpoint>();
    breakpoint->number = -1;
    breakpoint->line = line;
    breakpoint->filename = filename;
    breakpoint->condition = QString();
    breakpoint->enabled = true;
    breakpoint->breakpointType = BreakpointType::Breakpoint;
    breakpoint->timestamp = 0;
    breakpoint->fingerprint = contentContextFingerprint(content, line);
    model.addBreakpoint(breakpoint, forProject);
    return breakpoint;
}

// A formatter with a fixed result, used to drive the real reformat/replace path.
class StaticReformatter : public BaseReformatter
{
public:
    explicit StaticReformatter(QString content)
        : mContent{std::move(content)}
    {
    }

    QString reformat(const QString& /*content*/, QString& errorMessage, bool& isOk) override
    {
        errorMessage.clear();
        isOk = true;
        return mContent;
    }

private:
    QString mContent;
};

} // namespace

TestBreakpointRemap::TestBreakpointRemap(QObject *parent):
    TestEditorBase{parent}
{
}

// Two breakpoints on different lines of the same file: a merge removes the line
// between them, and undoing the merge (the line insertion, then the re-anchoring of
// all the breakpoints) puts each one back on its own line.
void TestBreakpointRemap::test_mergeAndUndoKeepsSeveralBreakpointsOnTheirLines()
{
    const QString filename = QStringLiteral("a.cpp");
    QStringList before({"int a(){","return 0;","}","int b(){","return 0;","}"});
    BreakpointModel model;
    PBreakpoint first = addBreakpoint(model, filename, before, 1);
    PBreakpoint second = addBreakpoint(model, filename, before, 4);
    QCOMPARE(model.breakpoints(false).count(), 2);

    // "int b(){" (line 3) is joined into the line above: the breakpoint below the
    // merge moves up with its line, the one above it stays
    model.onFileMergeLines(filename, 3, 2, false);
    model.onFileDeleteLines(filename, 3, 1, false);
    QCOMPARE(first->line, 1);
    QCOMPARE(second->line, 3);

    // Ctrl+Z: the line is inserted back, then the editor re-anchors every breakpoint
    model.onFileInsertLines(filename, 3, 1, false);
    model.reanchorBreakpoints(filename, before, false);
    QCOMPARE(model.breakpoints(false).count(), 2);
    QCOMPARE(first->line, 1);
    QCOMPARE(second->line, 4);
    QCOMPARE(first->fingerprint, contentContextFingerprint(before, 1));
    QCOMPARE(second->fingerprint, contentContextFingerprint(before, 4));
}

// The line bookkeeping of an ordinary edit moves every breakpoint below the edit,
// not just the first one.
void TestBreakpointRemap::test_addBreakpointOnAnEmptyLineSnapsToCode()
{
    mEditor->clearBreakpoints();
    mEditor->setContent({QStringLiteral("int main() {"),
                         QString(),
                         QStringLiteral("    return 0;"),
                         QStringLiteral("}")});
    mEditor->addBreakpoint(1);
    QVERIFY(mEditor->hasBreakpoint(2));
    QVERIFY(!mEditor->hasBreakpoint(1));
}

void TestBreakpointRemap::test_addBreakpointOnAnEmptyLineSkipsBracesAndComments()
{
    mEditor->clearBreakpoints();
    mEditor->setContent({QStringLiteral("int main() {"),
                         QString(),
                         QStringLiteral("}"),
                         QString(),
                         QStringLiteral("// a comment"),
                         QString(),
                         QStringLiteral("    work();"),
                         QStringLiteral("}")});
    mEditor->addBreakpoint(1);
    QVERIFY(mEditor->hasBreakpoint(6));
    QVERIFY(!mEditor->hasBreakpoint(1));
    QVERIFY(!mEditor->hasBreakpoint(2));
    QVERIFY(!mEditor->hasBreakpoint(4));
}

void TestBreakpointRemap::test_addBreakpointOnAnEmptyLineSkipsElseAndAccessSpecifiers()
{
    mEditor->clearBreakpoints();
    mEditor->setContent({QStringLiteral("class A {"),
                         QStringLiteral("public:"),
                         QString(),
                         QStringLiteral("else"),
                         QString(),
                         QStringLiteral("};"),
                         QStringLiteral("    work();"),
                         QStringLiteral("}")});
    mEditor->addBreakpoint(2);
    QVERIFY(mEditor->hasBreakpoint(6));
    QVERIFY(!mEditor->hasBreakpoint(1));
    QVERIFY(!mEditor->hasBreakpoint(3));
    QVERIFY(!mEditor->hasBreakpoint(5));
}

void TestBreakpointRemap::test_snapEmptyLineBreakpointMovesToCode()
{
    const QString filename = QStringLiteral("a.cpp");
    const QStringList content({QStringLiteral("int main() {"),
                               QString(),
                               QStringLiteral("    return 0;"),
                               QStringLiteral("}")});
    BreakpointModel model;
    PBreakpoint breakpoint = addBreakpoint(model, filename, content, 1);
    model.snapBreakpointsToCode(filename, content, false);
    QCOMPARE(breakpoint->line, 2);
    QCOMPARE(breakpoint->fingerprint, contentContextFingerprint(content, 2));

    // A breakpoint already on code is left alone.
    PBreakpoint codeBreakpoint = addBreakpoint(model, filename, content, 2);
    model.snapBreakpointsToCode(filename, content, false);
    QCOMPARE(codeBreakpoint->line, 2);
}

void TestBreakpointRemap::test_addBreakpointOnAnEmptyLineSkipsCaseDefaultAndLabels()
{
    mEditor->clearBreakpoints();
    mEditor->setContent({QStringLiteral("int f(int x) {"),
                         QStringLiteral("    switch (x) {"),
                         QStringLiteral("    case 1:"),
                         QString(),
                         QStringLiteral("    default:"),
                         QString(),
                         QStringLiteral("    label:"),
                         QString(),
                         QStringLiteral("        work();"),
                         QStringLiteral("    }"),
                         QStringLiteral("}")});
    mEditor->addBreakpoint(3);
    QVERIFY(mEditor->hasBreakpoint(8));
    QVERIFY(!mEditor->hasBreakpoint(2));
    QVERIFY(!mEditor->hasBreakpoint(4));
    QVERIFY(!mEditor->hasBreakpoint(6));
}

void TestBreakpointRemap::test_snapKeepsOtherDeletedBreakpointHistory()
{
    const QString filename = QStringLiteral("a.cpp");
    const QStringList content({QStringLiteral("int a;"),
                               QString(),
                               QStringLiteral("int b;"),
                               QStringLiteral("int c;")});
    BreakpointModel model;
    PBreakpoint empty = addBreakpoint(model, filename, content, 1);
    PBreakpoint deleted = addBreakpoint(model, filename, content, 3);

    model.onFileDeleteLines(filename, 3, 1, false);
    QCOMPARE(model.breakpoints(false).count(), 1);
    const QStringList afterDelete({QStringLiteral("int a;"),
                                   QString(),
                                   QStringLiteral("int b;")});
    model.snapBreakpointsToCode(filename, afterDelete, false);
    QCOMPARE(empty->line, 2);

    // The empty-line migration must not throw away the deletion history of a
    // different breakpoint.
    model.onFileInsertLines(filename, 3, 1, false, true);
    QCOMPARE(model.breakpoints(false).count(), 2);
    QCOMPARE(deleted->line, 3);
}

void TestBreakpointRemap::test_addBreakpointOnAnEmptyLineSkipsLabelsWithComments()
{
    mEditor->clearBreakpoints();
    mEditor->setContent({QStringLiteral("int f(int x) {"),
                         QStringLiteral("    switch (x) {"),
                         QStringLiteral("    case 1: // first"),
                         QString(),
                         QStringLiteral("    default: // fallback"),
                         QString(),
                         QStringLiteral("    label: // target"),
                         QString(),
                         QStringLiteral("        work();"),
                         QStringLiteral("    }"),
                         QStringLiteral("}")});
    mEditor->addBreakpoint(3);
    QVERIFY(mEditor->hasBreakpoint(8));
    QVERIFY(!mEditor->hasBreakpoint(2));
    QVERIFY(!mEditor->hasBreakpoint(4));
    QVERIFY(!mEditor->hasBreakpoint(6));
}

void TestBreakpointRemap::test_addBreakpointOnAnEmptyLineAcceptsOneLineCase()
{
    mEditor->clearBreakpoints();
    mEditor->setContent({QStringLiteral("int f(int x) {"),
                         QStringLiteral("    switch (x) {"),
                         QString(),
                         QStringLiteral("    case 1: work();"),
                         QStringLiteral("    }"),
                         QStringLiteral("}")});
    mEditor->addBreakpoint(2);
    QVERIFY(mEditor->hasBreakpoint(3));
    QVERIFY(!mEditor->hasBreakpoint(2));
}

void TestBreakpointRemap::test_addBreakpointOnAnEmptyLineSkipsBlockCommentLabels()
{
    mEditor->clearBreakpoints();
    mEditor->setContent({QStringLiteral("int f(int x) {"),
                         QStringLiteral("    switch (x) {"),
                         QStringLiteral("    case 1: /* first */"),
                         QString(),
                         QStringLiteral("    default: /* fallback */"),
                         QString(),
                         QStringLiteral("    label: /* target */"),
                         QString(),
                         QStringLiteral("        work();"),
                         QStringLiteral("    }"),
                         QStringLiteral("}")});
    mEditor->addBreakpoint(3);
    QVERIFY(mEditor->hasBreakpoint(8));
    QVERIFY(!mEditor->hasBreakpoint(2));
    QVERIFY(!mEditor->hasBreakpoint(4));
    QVERIFY(!mEditor->hasBreakpoint(6));
}

void TestBreakpointRemap::test_addBreakpointOnAnEmptyLineSkipsMultilineBlockComment()
{
    mEditor->clearBreakpoints();
    mEditor->setContent({QStringLiteral("int f() {"),
                         QStringLiteral("/*"),
                         QStringLiteral("case 1:"),
                         QStringLiteral("default:"),
                         QStringLiteral("*/"),
                         QString(),
                         QStringLiteral("    work();"),
                         QStringLiteral("}")});
    mEditor->addBreakpoint(5);
    QVERIFY(mEditor->hasBreakpoint(6));
    QVERIFY(!mEditor->hasBreakpoint(2));
    QVERIFY(!mEditor->hasBreakpoint(3));
}

void TestBreakpointRemap::test_addBreakpointOnAnEmptyLineAcceptsOneLineCaseWithBlockComment()
{
    mEditor->clearBreakpoints();
    mEditor->setContent({QStringLiteral("int f(int x) {"),
                         QStringLiteral("    switch (x) {"),
                         QString(),
                         QStringLiteral("    case 1: /* one line */ work();"),
                         QStringLiteral("    }"),
                         QStringLiteral("}")});
    mEditor->addBreakpoint(2);
    QVERIFY(mEditor->hasBreakpoint(3));
    QVERIFY(!mEditor->hasBreakpoint(2));
}

void TestBreakpointRemap::test_addBreakpointOnAnEmptyLineKeepsSlashString()
{
    mEditor->clearBreakpoints();
    mEditor->setContent({QStringLiteral("int f() {"),
                         QString(),
                         QStringLiteral("    const char* s = \"a//b\";"),
                         QStringLiteral("    work();"),
                         QStringLiteral("}")});
    mEditor->addBreakpoint(1);
    QVERIFY(mEditor->hasBreakpoint(2));
    QVERIFY(!mEditor->hasBreakpoint(3));
}

void TestBreakpointRemap::test_addBreakpointOnAnEmptyLineKeepsRawString()
{
    mEditor->clearBreakpoints();
    mEditor->setContent({QStringLiteral("int f() {"),
                         QString(),
                         QStringLiteral("    const char* s = R\"(a/*b*/)//c\";"),
                         QStringLiteral("    work();"),
                         QStringLiteral("}")});
    mEditor->addBreakpoint(1);
    QVERIFY(mEditor->hasBreakpoint(2));
    QVERIFY(!mEditor->hasBreakpoint(3));
}

void TestBreakpointRemap::test_snapOnlyNotifiesTheActiveModel()
{
    const QString filename = QStringLiteral("a.cpp");
    const QStringList content({QStringLiteral("int a;"),
                               QString(),
                               QString(),
                               QStringLiteral("int b;")});
    BreakpointModel model;
    PBreakpoint normal = addBreakpoint(model, filename, content, 1, false);
    PBreakpoint project = addBreakpoint(model, filename, content, 1, true);
    QSignalSpy spy(&model, SIGNAL(dataChanged(QModelIndex,QModelIndex)));

    const QStringList afterDelete({QStringLiteral("int a;"),
                                   QString(),
                                   QStringLiteral("int b;")});
    model.snapBreakpointsToCode(filename, afterDelete, false);

    QCOMPARE(normal->line, 2);
    QCOMPARE(project->line, 2);
    // The model is showing the non-project list; changing the project list
    // must not emit a row update against the visible rows.
    QCOMPARE(spy.count(), 1);
}

void TestBreakpointRemap::test_updateBreakpointNumberUsesCondition()
{
    const QString filename = QStringLiteral("a.cpp");
    const QStringList content({QStringLiteral("a;"), QStringLiteral("b;")});
    BreakpointModel model;
    PBreakpoint first = addBreakpoint(model, filename, content, 1);
    PBreakpoint second = addBreakpoint(model, filename, content, 1);
    first->condition = QStringLiteral("a == 2");
    second->condition = QStringLiteral("b == 3");

    // GDB may return the same condition without the spaces the user typed.
    model.updateBreakpointNumber(filename, 1, 7, QStringLiteral("b==3"));
    model.updateBreakpointNumber(filename, 1, 8, QStringLiteral("a==2"));

    QCOMPARE(first->number, 8);
    QCOMPARE(second->number, 7);

    // Equal conditions are inherently ambiguous; define the fallback as model
    // order and keep it stable.
    model.invalidateAllBreakpointNumbers();
    first->condition = QStringLiteral("same");
    second->condition = QStringLiteral("same");
    model.updateBreakpointNumber(filename, 1, 21, QStringLiteral("same"));
    model.updateBreakpointNumber(filename, 1, 22, QStringLiteral("same"));
    QCOMPARE(first->number, 21);
    QCOMPARE(second->number, 22);
}

void TestBreakpointRemap::test_snapMigratesBothModelsAndKeepsDeletions()
{
    const QString filename = QStringLiteral("a.cpp");
    const QStringList content({QStringLiteral("int a;"),
                               QString(),
                               QStringLiteral("int b;"),
                               QStringLiteral("int c;")});
    BreakpointModel model;
    PBreakpoint normal = addBreakpoint(model, filename, content, 1, false);
    PBreakpoint project = addBreakpoint(model, filename, content, 1, true);
    PBreakpoint deleted = addBreakpoint(model, filename, content, 3, false);

    model.onFileDeleteLines(filename, 3, 1, false);
    const QStringList afterDelete({QStringLiteral("int a;"),
                                   QString(),
                                   QStringLiteral("int b;")});
    model.snapBreakpointsToCode(filename, afterDelete, false);
    QCOMPARE(normal->line, 2);
    QCOMPARE(project->line, 2);
    QCOMPARE(model.breakpoints(false).count(), 1);
    QCOMPARE(model.breakpoints(true).count(), 1);

    model.onFileInsertLines(filename, 3, 1, false, true);
    QCOMPARE(deleted->line, 3);
    QCOMPARE(model.breakpoints(false).count(), 2);
}

void TestBreakpointRemap::test_snapKeepsSeveralDeletedBreakpoints()
{
    const QString filename = QStringLiteral("a.cpp");
    const QStringList content({QStringLiteral("int a;"),
                               QString(),
                               QStringLiteral("int b;"),
                               QStringLiteral("int c;"),
                               QStringLiteral("int d;")});
    BreakpointModel model;
    PBreakpoint empty = addBreakpoint(model, filename, content, 1);
    PBreakpoint firstDeleted = addBreakpoint(model, filename, content, 3);
    PBreakpoint secondDeleted = addBreakpoint(model, filename, content, 4);

    model.onFileDeleteLines(filename, 3, 2, false);
    const QStringList afterDelete({QStringLiteral("int a;"),
                                   QString(),
                                   QStringLiteral("int b;")});
    model.snapBreakpointsToCode(filename, afterDelete, false);
    QCOMPARE(empty->line, 2);

    model.onFileInsertLines(filename, 3, 2, false, true);
    QCOMPARE(model.breakpoints(false).count(), 3);
    QCOMPARE(firstDeleted->line, 3);
    QCOMPARE(secondDeleted->line, 4);
}

void TestBreakpointRemap::test_snapRemovesMigratedBreakpointFromItsOwnHistory()
{
    const QString filename = QStringLiteral("a.cpp");
    const QStringList content({QStringLiteral("int a;"),
                               QString(),
                               QString(),
                               QStringLiteral("int b;")});
    BreakpointModel model;
    PBreakpoint empty = addBreakpoint(model, filename, content, 1);

    model.onFileDeleteLines(filename, 1, 1, false);
    const QStringList afterDelete({QStringLiteral("int a;"),
                                   QString(),
                                   QStringLiteral("int b;")});
    // Re-add the same object while its deletion history is still present.
    model.addBreakpoint(empty, false);
    model.snapBreakpointsToCode(filename, afterDelete, false);
    QCOMPARE(empty->line, 2);

    // The stale history entry for this object must not restore a duplicate.
    model.onFileInsertLines(filename, 1, 1, false, true);
    QCOMPARE(model.breakpoints(false).count(), 1);
    QCOMPARE(empty->line, 3);
}

void TestBreakpointRemap::test_snapKeepsDeletionHistoryForBothModels()
{
    const QString filename = QStringLiteral("a.cpp");
    const QStringList content({QStringLiteral("int a;"),
                               QString(),
                               QStringLiteral("int b;"),
                               QStringLiteral("int c;")});
    BreakpointModel model;
    PBreakpoint normalEmpty = addBreakpoint(model, filename, content, 1, false);
    PBreakpoint projectEmpty = addBreakpoint(model, filename, content, 1, true);
    PBreakpoint normalDeleted = addBreakpoint(model, filename, content, 3, false);
    PBreakpoint projectDeleted = addBreakpoint(model, filename, content, 3, true);

    model.onFileDeleteLines(filename, 3, 1, false);
    model.onFileDeleteLines(filename, 3, 1, true);
    const QStringList afterDelete({QStringLiteral("int a;"),
                                   QString(),
                                   QStringLiteral("int b;")});
    model.snapBreakpointsToCode(filename, afterDelete, false);
    QCOMPARE(normalEmpty->line, 2);
    QCOMPARE(projectEmpty->line, 2);

    model.onFileInsertLines(filename, 3, 1, false, true);
    model.onFileInsertLines(filename, 3, 1, true, true);
    QCOMPARE(model.breakpoints(false).count(), 2);
    QCOMPARE(model.breakpoints(true).count(), 2);
    QCOMPARE(normalDeleted->line, 3);
    QCOMPARE(projectDeleted->line, 3);
}

void TestBreakpointRemap::test_updateBreakpointNumberForSeveralBreakpointsOnALine()
{
    const QString filename = QStringLiteral("a.cpp");
    const QStringList content({QStringLiteral("a;"), QStringLiteral("b;")});
    BreakpointModel model;
    PBreakpoint first = addBreakpoint(model, filename, content, 1);
    PBreakpoint second = addBreakpoint(model, filename, content, 1);

    model.updateBreakpointNumber(filename, 1, 1);
    model.updateBreakpointNumber(filename, 1, 2);

    QCOMPARE(first->number, 1);
    QCOMPARE(second->number, 2);
}

void TestBreakpointRemap::test_lineSignalsKeepSeveralBreakpointsInStep()
{
    const QString filename = QStringLiteral("a.cpp");
    QStringList content({"a;","b;","c;","d;","e;"});
    BreakpointModel model;
    PBreakpoint first = addBreakpoint(model, filename, content, 1);
    PBreakpoint second = addBreakpoint(model, filename, content, 4);

    // a line is inserted above both breakpoints
    model.onFileInsertLines(filename, 0, 1, false);
    QCOMPARE(first->line, 2);
    QCOMPARE(second->line, 5);

    // undoing it removes the line again
    model.onFileDeleteLines(filename, 0, 1, false);
    QCOMPARE(first->line, 1);
    QCOMPARE(second->line, 4);
}

// A whole content replacement (a formatting) moves the lines: re-anchoring must put
// every breakpoint of the file back on its own code.
void TestBreakpointRemap::test_reanchorAfterAReflowKeepsSeveralBreakpoints()
{
    const QString filename = QStringLiteral("a.cpp");
    QStringList before({"int a(){","return 0;","}","int b(){","return 0;","}"});
    BreakpointModel model;
    PBreakpoint first = addBreakpoint(model, filename, before, 1);
    PBreakpoint second = addBreakpoint(model, filename, before, 4);

    QStringList formatted({"// header","int a(){","    return 0;","}",
                           "int b(){","    return 0;","}"});
    model.reanchorBreakpoints(filename, formatted, false);
    QCOMPARE(first->line, 2);
    QCOMPARE(second->line, 5);
    QCOMPARE(first->fingerprint, contentContextFingerprint(formatted, 2));
    QCOMPARE(second->fingerprint, contentContextFingerprint(formatted, 5));
}

// The real editor and the real model together: two breakpoints on different lines, a
// line merged between them, then Ctrl+Z. The line reports move the model, and the
// split re-anchors every breakpoint; each one must be back on its own line.
void TestBreakpointRemap::test_editor_mergeAndUndoKeepsSeveralBreakpoints()
{
    const QString filename = QStringLiteral("a.cpp");
    QStringList content({"int a(){","return 0;","}","int b(){","return 0;","}"});
    mEditor->setFilename(filename);
    mEditor->setContent(content);
    BreakpointModel model;
    PBreakpoint first = addBreakpoint(model, filename, content, 1);
    PBreakpoint second = addBreakpoint(model, filename, content, 4);
    mEditor->resetBreakpoints(&model);
    QVERIFY(mEditor->hasBreakpoint(1));
    QVERIFY(mEditor->hasBreakpoint(4));

    // what EditorManager does with the line signals of the editor
    connect(mEditor.get(), &QSynedit::QSynEdit::linesInserted, this,
            [this, &model](int startLine, int count) {
                if (mEditor->isReplacingContent())
                    return;
                model.onFileInsertLines(mEditor->filename(), startLine, count,
                                        mEditor->inProject());
                mEditor->resetBreakpoints(&model);
            });
    connect(mEditor.get(), &QSynedit::QSynEdit::linesDeleted, this,
            [this, &model](int startLine, int count) {
                if (mEditor->isReplacingContent())
                    return;
                model.onFileDeleteLines(mEditor->filename(), startLine, count,
                                        mEditor->inProject());
                mEditor->resetBreakpoints(&model);
            });
    connect(mEditor.get(), &QSynedit::QSynEdit::linesMerged, this,
            [this, &model](int removedLine, int intoLine) {
                model.onFileMergeLines(mEditor->filename(), removedLine, intoLine,
                                       mEditor->inProject());
                mEditor->resetBreakpoints(&model);
            });
    connect(mEditor.get(), &QSynedit::QSynEdit::linesSplit, this,
            [this, &model](int /*mergedLine*/, int /*newLine*/) {
                model.reanchorBreakpoints(mEditor->filename(), mEditor->content(),
                                          mEditor->inProject());
                mEditor->resetBreakpoints(&model);
            });

    // join "}" (line 2) with "int b(){" (line 3)
    mEditor->setCaretXY(QSynedit::CharPos{1, 2});
    QTest::keyPress(mEditor.get(), Qt::Key_Delete);
    QCOMPARE(mEditor->content(),
             QStringList({"int a(){","return 0;","}int b(){","return 0;","}"}));
    QCOMPARE(first->line, 1);
    QCOMPARE(second->line, 3);

    // Ctrl+Z: the line is inserted back and split, and every breakpoint finds its code
    mEditor->undo();
    QCOMPARE(mEditor->content(), content);
    QCOMPARE(first->line, 1);
    QCOMPARE(second->line, 4);
    QVERIFY(mEditor->hasBreakpoint(1));
    QVERIFY(mEditor->hasBreakpoint(4));
    disconnect(mEditor.get(), nullptr, this, nullptr);
}

// A whole content replacement (a formatting) and its undo, through the real editor:
// the replacement re-anchors both breakpoints, and the undo puts each one back on its
// own line.
void TestBreakpointRemap::test_editor_reformatUndoKeepsSeveralBreakpoints()
{
    const QString filename = QStringLiteral("a.cpp");
    QStringList before({"int a(){","return 0;","}","int b(){","return 0;","}"});
    mEditor->setFilename(filename);
    mEditor->setContent(before);
    BreakpointModel model;
    PBreakpoint first = addBreakpoint(model, filename, before, 1);
    PBreakpoint second = addBreakpoint(model, filename, before, 4);
    mEditor->resetBreakpoints(&model);
    mEditor->setContentReplacedFunc([this, &model](const QString& filename, bool inProject,
                                                   const QStringList& content,
                                                   const QMap<int,int>& /*lineMap*/) {
        model.reanchorBreakpoints(filename, content, inProject);
        mEditor->resetBreakpoints(&model);
    });

    QStringList formatted({"// header","int a(){","    return 0;","}",
                           "int b(){","    return 0;","}"});
    mEditor->replaceContent(formatted.join(QLatin1Char('\n')));
    QCOMPARE(mEditor->content(), formatted);
    QCOMPARE(first->line, 2);
    QCOMPARE(second->line, 5);

    // the undo of the whole replacement re-anchors the breakpoints on the old content
    mEditor->undo();
    QCOMPARE(mEditor->content(), before);
    QCOMPARE(first->line, 1);
    QCOMPARE(second->line, 4);
    QVERIFY(mEditor->hasBreakpoint(1));
    QVERIFY(mEditor->hasBreakpoint(4));
    mEditor->setContentReplacedFunc(nullptr);
}

// The real reformat path (a formatter result applied by Editor::reformat()) keeps a
// breakpoint and a bookmark on the code of a short line that the formatter attaches
// to the closing brace above it ("} else {"): this is what clang-format/astyle do
// with braces and else clauses.
void TestBreakpointRemap::test_editor_reformatKeepsBreakpointAndBookmark()
{
    const QString filename = QStringLiteral("a.cpp");
    QStringList before({"// header","int main() {","if (a) {","x();","}","else {","y();","}"});
    QStringList formatted({"// header","int main() {","if (a) {","x();","} else {","y();","}"});
    mEditor->setFilename(filename);
    mEditor->setFileType(FileType::CppSource, false);
    mEditor->setContent(before);

    BreakpointModel breakpoints;
    PBreakpoint breakpoint = addBreakpoint(breakpoints, filename, before, 5);
    BookmarkModel bookmarks;
    bookmarks.addBookmark(filename, 5, QString(), false);
    mEditor->resetBreakpoints(&breakpoints);
    mEditor->resetBookmarks(&bookmarks);
    mEditor->setContentReplacedFunc([this, &breakpoints, &bookmarks](
                                        const QString& filename, bool inProject,
                                        const QStringList& content,
                                        const QMap<int,int>& lineMap) {
        breakpoints.reanchorBreakpoints(filename, content, inProject);
        bookmarks.moveBookmarksInFile(filename, lineMap, content.count(), inProject,
                                      mEditor->isRestoringContentByUndoRedo());
        mEditor->resetBreakpoints(&breakpoints);
        mEditor->resetBookmarks(&bookmarks);
    });
    mEditor->setGetReformatterFunc([formatted](Editor*) {
        return std::make_unique<StaticReformatter>(formatted.join(QLatin1Char('\n')));
    });

    mEditor->reformat(false, false, false);

    QCOMPARE(mEditor->content(), formatted);
    QCOMPARE(breakpoint->line, 4);
    QVERIFY(mEditor->hasBreakpoint(4));
    QVERIFY(bookmarks.bookmark(filename, 4, false) != nullptr);
    QVERIFY(mEditor->hasBookmark(4));

    mEditor->setGetReformatterFunc(nullptr);
    mEditor->setContentReplacedFunc(nullptr);
}

// The same, with the real clang-format and astyle binaries. The test is skipped when
// they are not installed; set RED_PANDA_FORMATTER_DIR to the directory holding them.
void TestBreakpointRemap::test_editor_realFormattersKeepBreakpointAndBookmark()
{
    const QString formatterDir = qEnvironmentVariable(
                "RED_PANDA_FORMATTER_DIR",
                QStringLiteral("C:/Users/Administrator/Desktop/format"));
    auto findTool = [&formatterDir](const QString& base) {
        const QStringList names{base + QStringLiteral(".exe"), base};
        for (const QString& name: names) {
            const QString path = QDir(formatterDir).filePath(name);
            if (QFileInfo(path).isExecutable())
                return path;
        }
        return QString();
    };
    const QString clangFormat = findTool(QStringLiteral("clang-format"));
    const QString aStyle = findTool(QStringLiteral("astyle"));
    if (clangFormat.isEmpty() || aStyle.isEmpty())
        QSKIP("clang-format/astyle not found");

    const QString filename = QDir::temp().filePath(QStringLiteral("redpanda-format-test.cpp"));
    QStringList before({
        "#include <cstdio>",
        "int main() {",
        "if (a) {",
        "x();",
        "}",
        "else {",
        "y();",
        "}",
        "int veryLongVariableName = computeSomethingWithAVeryLongName(1, 2, 3, 4, 5, 6);",
        "return 0;",
        "}"});

    auto findLine = [](const QStringList& content, const QString& code) {
        for (int line=0;line<content.count();line++) {
            if (breakpointFingerprint(content[line]).contains(code))
                return line;
        }
        return -1;
    };

    auto runCase = [&](const QString& name,
                       std::function<std::unique_ptr<BaseReformatter>(Editor*)> factory) {
        mEditor->setFilename(filename);
        mEditor->setFileType(FileType::CppSource, false);
        mEditor->setContent(before);
        BreakpointModel breakpoints;
        PBreakpoint onElse = addBreakpoint(breakpoints, filename, before, 5);
        PBreakpoint onReturn = addBreakpoint(breakpoints, filename, before, 9);
        BookmarkModel bookmarks;
        bookmarks.addBookmark(filename, 5, QString(), false);
        bookmarks.addBookmark(filename, 9, QString(), false);
        mEditor->resetBreakpoints(&breakpoints);
        mEditor->resetBookmarks(&bookmarks);
        mEditor->setContentReplacedFunc([this, &breakpoints, &bookmarks](
                                            const QString& filename, bool inProject,
                                            const QStringList& content,
                                            const QMap<int,int>& lineMap) {
            breakpoints.reanchorBreakpoints(filename, content, inProject);
            bookmarks.moveBookmarksInFile(filename, lineMap, content.count(), inProject,
                                          mEditor->isRestoringContentByUndoRedo());
            mEditor->resetBreakpoints(&breakpoints);
            mEditor->resetBookmarks(&bookmarks);
        });
        mEditor->setGetReformatterFunc(factory);
        mEditor->reformat(false, false, false);
        // a formatter that failed would leave the content unchanged; the checks below
        // would then pass on the untouched markers, so make sure it really formatted
        QVERIFY2(mEditor->content() != before, qPrintable(name + ": the formatter did not format"));

        const int elseLine = findLine(mEditor->content(), QStringLiteral("else{"));
        const int returnLine = findLine(mEditor->content(), QStringLiteral("return0;"));
        QVERIFY2(elseLine >= 0, qPrintable(name));
        QVERIFY2(returnLine >= 0, qPrintable(name));
        QCOMPARE(onElse->line, elseLine);
        QCOMPARE(onReturn->line, returnLine);
        QVERIFY(mEditor->hasBreakpoint(elseLine));
        QVERIFY(mEditor->hasBreakpoint(returnLine));
        QVERIFY(bookmarks.bookmark(filename, elseLine, false));
        QVERIFY(bookmarks.bookmark(filename, returnLine, false));
        QVERIFY(mEditor->hasBookmark(elseLine));
        QVERIFY(mEditor->hasBookmark(returnLine));

        mEditor->setGetReformatterFunc(nullptr);
        mEditor->setContentReplacedFunc(nullptr);
    };

    runCase(QStringLiteral("clang-format"),
            [clangFormat, filename](Editor*) {
        return std::make_unique<ClangFormatReformatter>(
                    clangFormat, QStringList{QStringLiteral("-style=LLVM")},
                    filename, QDir::tempPath(), LoggerFunc(), nullptr, 10000);
    });
    runCase(QStringLiteral("astyle"),
            [aStyle, filename](Editor*) {
        return std::make_unique<AStyleReformatter>(
                    aStyle,
                    QStringList{QStringLiteral("-I"), QStringLiteral("--style=java"),
                                QStringLiteral("--indent=spaces=4")},
                    filename, QDir::tempPath(), LoggerFunc(), nullptr, 10000);
    });
}

// A rename is about the file, not one model: both the project and the non-project
// breakpoint lists follow it.
void TestBreakpointRemap::test_renameUpdatesBothModels()
{
    const QString oldName = QStringLiteral("a.cpp");
    const QString newName = QStringLiteral("b.cpp");
    const QStringList content({"a;","b;"});
    BreakpointModel model;
    PBreakpoint nonProject = addBreakpoint(model, oldName, content, 1, false);
    PBreakpoint project = addBreakpoint(model, oldName, content, 1, true);

    model.renameBreakpointFilenames(oldName, newName, false);

    QCOMPARE(nonProject->filename, newName);
    QCOMPARE(project->filename, newName);
    QCOMPARE(model.breakpoints(false).count(), 1);
    QCOMPARE(model.breakpoints(true).count(), 1);
    QCOMPARE(model.breakpoints(false)[0]->filename, newName);
    QCOMPARE(model.breakpoints(true)[0]->filename, newName);
}

// A formatter can join the continuation lines of a split statement.  The
// breakpoints of two continuation lines must not both be re-anchored to the
// joined line, or undoing the format loses one of them (the gutter only draws
// one marker for a line).
void TestBreakpointRemap::test_reanchorKeepsTheAnchorsOfSplitContinuationLines()
{
    const QString filename = QStringLiteral("a.cpp");
    const QStringList split = {
        QStringLiteral("#include <iostream>"),
        QStringLiteral("// #include <stdlib.h>"),
        QStringLiteral("// #include <math.h>"),
        QStringLiteral("using namespace std;"),
        QStringLiteral("int main() {"),
        QStringLiteral("    int i, j;"),
        QStringLiteral("    for (i = 1;"),
        QStringLiteral("         i < 10; i++) {"),
        QStringLiteral("        for (j = 1;"),
        QStringLiteral("             j <= i; j++) {"),
        QStringLiteral("            cout << i << \"*\""),
        QStringLiteral("                 << j << \"=\""),
        QStringLiteral("                 << i * j << \" \";"),
        QStringLiteral("        }"),
        QStringLiteral("        cout << \"\n\";"),
        QStringLiteral("    }"),
        QStringLiteral("}")
    };
    const QStringList collapsed = {
        QStringLiteral("#include <iostream>"),
        QStringLiteral("// #include <stdlib.h>"),
        QStringLiteral("// #include <math.h>"),
        QStringLiteral("using namespace std;"),
        QStringLiteral("int main() {"),
        QStringLiteral("    int i, j;"),
        QStringLiteral("    for (i = 1; i < 10; i++) {"),
        QStringLiteral("        for (j = 1; j <= i; j++) {"),
        QStringLiteral("            cout << i << \"*\" << j << \"=\" << i * j << \" \";"),
        QStringLiteral("        }"),
        QStringLiteral("        cout << \"\n\";"),
        QStringLiteral("    }"),
        QStringLiteral("}")
    };
    BreakpointModel model;
    PBreakpoint onForI = addBreakpoint(model, filename, split, 6);
    PBreakpoint onForJ = addBreakpoint(model, filename, split, 8);
    PBreakpoint onCout = addBreakpoint(model, filename, split, 10);

    // clang-format joins the continuation lines
    model.reanchorBreakpoints(filename, collapsed, false);
    QCOMPARE(onForI->line, 6);
    QCOMPARE(onForJ->line, 7);
    QCOMPARE(onCout->line, 8);

    // Ctrl+Z brings the original split lines back; every breakpoint must return
    // to its own line, not two of them to the same joined line.
    model.reanchorBreakpoints(filename, split, false);
    QCOMPARE(onForI->line, 6);
    QCOMPARE(onForJ->line, 8);
    QCOMPARE(onCout->line, 10);
}

// Two independent lines can be joined by a formatter.  The model allows several
// breakpoints on one line, so both breakpoints must survive (not disappear).
void TestBreakpointRemap::test_reanchorKeepsBothIndependentBreakpointsOnACollision()
{
    const QString filename = QStringLiteral("a.cpp");
    const QStringList before = {QStringLiteral("a;"), QStringLiteral("b;"),
                                QStringLiteral("c;")};
    const QStringList after = {QStringLiteral("a;c;")};
    BreakpointModel model;
    PBreakpoint first = addBreakpoint(model, filename, before, 0);
    PBreakpoint second = addBreakpoint(model, filename, before, 2);

    model.reanchorBreakpoints(filename, after, false);

    QCOMPARE(model.breakpoints(false).count(), 2);
    QCOMPARE(first->line, 0);
    QCOMPARE(second->line, 0);
}

// A line deletion removes the breakpoint of that line; undoing the deletion puts
// it back on its line, while the breakpoints below follow the inserted line again.
void TestBreakpointRemap::test_undoOfALineDeletionRestoresItsBreakpoint()
{
    const QString filename = QStringLiteral("a.cpp");
    const QStringList content({"a;","b;","c;","d;"});
    BreakpointModel model;
    PBreakpoint first = addBreakpoint(model, filename, content, 1);
    PBreakpoint second = addBreakpoint(model, filename, content, 3);

    model.onFileDeleteLines(filename, 1, 1, false);
    QCOMPARE(model.breakpoints(false).count(), 1);
    QVERIFY(!model.breakpoints(false).contains(first));
    QCOMPARE(second->line, 2);

    // Ctrl+Z: the line is inserted back, this is an undo of the deletion
    model.onFileInsertLines(filename, 1, 1, false, true);

    QCOMPARE(model.breakpoints(false).count(), 2);
    QVERIFY(model.breakpoints(false).contains(first));
    QCOMPARE(first->line, 1);
    QCOMPARE(second->line, 3);
}

// A plain insertion at the same place is not an undo: it must not resurrect the
// breakpoint that the line deletion removed.
void TestBreakpointRemap::test_plainInsertionDoesNotRestoreADeletedBreakpoint()
{
    const QString filename = QStringLiteral("a.cpp");
    const QStringList content({"a;","b;","c;","d;"});
    BreakpointModel model;
    PBreakpoint first = addBreakpoint(model, filename, content, 1);
    PBreakpoint second = addBreakpoint(model, filename, content, 3);

    model.onFileDeleteLines(filename, 1, 1, false);
    model.onFileInsertLines(filename, 1, 1, false, false);

    QCOMPARE(model.breakpoints(false).count(), 1);
    QVERIFY(!model.breakpoints(false).contains(first));
    QCOMPARE(second->line, 3);
}


// The breakpoint toggle is routed to the application-level history instead of
// undoing the text when it is the last action.
void TestBreakpointRemap::test_breakpointToggleUsesApplicationUndo()
{
    mEditor->setContent(QStringList({"a;","b;"}));
    int undoCount = 0;
    int redoCount = 0;
    mEditor->setBreakpointHistoryFuncs(
                [&undoCount](Editor*) { ++undoCount; return true; },
                [&redoCount](Editor*) { ++redoCount; return true; });
    mEditor->addBreakpoint(1);
    mEditor->undo();
    QCOMPARE(undoCount, 1);
    mEditor->redo();
    QCOMPARE(redoCount, 1);
    mEditor->setBreakpointHistoryFuncs(nullptr, nullptr);
}
