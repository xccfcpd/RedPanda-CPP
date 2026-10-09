#include "test_editor_anchors.h"

#include "src/editoranchors.h"
#include "src/debugger/debuggermodels.h"

#include <QTest>

TestEditorAnchors::TestEditorAnchors(QObject *parent):
    QObject{parent}
{
}

// The anchors of the given lines of "content", taken as the editor takes them
// before it replaces the whole content of a document.
static QMap<int,ReformatAnchor> anchorsOf(const QStringList& content, const QList<int>& lines)
{
    QMap<int,ReformatAnchor> result;
    foreach (int line, lines) {
        result.insert(line, EditorAnchors::lineAnchor(
                               line>0? content[line-1] : QString(),
                               content[line],
                               line+1<content.count()? content[line+1] : QString()));
    }
    return result;
}

void TestEditorAnchors::test_lineKey_keepsOnlyNonWhitespace()
{
    QCOMPARE(EditorAnchors::lineKey(QStringLiteral("  int\t x = 1;  ")),
             QStringLiteral("intx=1;"));
    QCOMPARE(EditorAnchors::lineKey(QString()), QString());
    QCOMPARE(EditorAnchors::lineKey(QStringLiteral("   \t  ")), QString());
}

void TestEditorAnchors::test_lineKey_capsLongLines()
{
    // capped like the fingerprint of a breakpoint, so that a session file stays small
    QCOMPARE(EditorAnchors::lineKey(QString(199, QLatin1Char('a'))).length(), 199);
    QCOMPARE(EditorAnchors::lineKey(QString(200, QLatin1Char('a'))).length(), 200);
    QCOMPARE(EditorAnchors::lineKey(QString(300, QLatin1Char('a'))).length(), 200);
}

void TestEditorAnchors::test_lineKey_capsAfterRemovingWhitespace()
{
    // the whitespace is removed first: a line broken by spaces has the same key as
    // the same line written in one piece
    QString spaced = QString(150, QLatin1Char('a'))
                     + QStringLiteral("    ")
                     + QString(150, QLatin1Char('b'));
    QString compact = QString(150, QLatin1Char('a')) + QString(50, QLatin1Char('b'));
    QCOMPARE(EditorAnchors::lineKey(spaced), EditorAnchors::lineKey(compact));
}

void TestEditorAnchors::test_lineAnchor_contextHoldsTheNeighbours()
{
    ReformatAnchor anchor = EditorAnchors::lineAnchor(QStringLiteral("  a();"),
                                                      QStringLiteral("  b();"),
                                                      QStringLiteral("  c();"));
    QCOMPARE(anchor.line, QStringLiteral("b();"));
    QCOMPARE(anchor.context, QStringLiteral("a();\nb();\nc();"));
    QVERIFY(anchor.line != anchor.context);
}

void TestEditorAnchors::test_lineAnchor_firstAndLastLine()
{
    // a line outside of the document is an empty text
    QCOMPARE(EditorAnchors::lineAnchor(QString(), QStringLiteral("a"), QStringLiteral("b")).context,
             QStringLiteral("\na\nb"));
    QCOMPARE(EditorAnchors::lineAnchor(QStringLiteral("a"), QStringLiteral("b"), QString()).context,
             QStringLiteral("a\nb\n"));
}

void TestEditorAnchors::test_lineAnchor_matchesTheBreakpointAnchor()
{
    // the models anchor a breakpoint with breakpointContextFingerprint() and the
    // editor anchors a line with EditorAnchors::lineAnchor(): the two must agree, or
    // the same line would be matched differently in the two of them
    QCOMPARE(EditorAnchors::lineAnchor(QStringLiteral("  a();"),
                                       QStringLiteral(" b(); "),
                                       QStringLiteral("c();  ")).context,
             breakpointContextFingerprint(QStringLiteral("  a();"),
                                          QStringLiteral(" b(); "),
                                          QStringLiteral("c();  ")));
    QCOMPARE(EditorAnchors::lineKey(QStringLiteral("  a();")),
             breakpointFingerprint(QStringLiteral("  a();")));
}

void TestEditorAnchors::test_remap_contentUnchanged()
{
    QStringList content = {QStringLiteral("int main() {"),
                           QStringLiteral("    return 0;"),
                           QStringLiteral("}")};
    QMap<int,int> map = EditorAnchors::remapLines(anchorsOf(content, {0, 1, 2}), content);
    QCOMPARE(map.value(0), 0);
    QCOMPARE(map.value(1), 1);
    QCOMPARE(map.value(2), 2);
}

void TestEditorAnchors::test_remap_linesInsertedBeforeTheAnchor()
{
    QStringList oldContent = {QStringLiteral("a = 1;"),
                              QStringLiteral("b = 2;"),
                              QStringLiteral("c = 3;"),
                              QStringLiteral("d = 4;")};
    QMap<int,ReformatAnchor> anchors = anchorsOf(oldContent, {2});
    QStringList newContent = {QStringLiteral("// header"),
                              QStringLiteral("// more"),
                              QStringLiteral("a = 1;"),
                              QStringLiteral("b = 2;"),
                              QStringLiteral("c = 3;"),
                              QStringLiteral("d = 4;")};
    QMap<int,int> map = EditorAnchors::remapLines(anchors, newContent);
    QCOMPARE(map.value(2), 4);
}

void TestEditorAnchors::test_remap_linesRemovedBeforeTheAnchor()
{
    QStringList oldContent = {QStringLiteral("// header"),
                              QStringLiteral("// more"),
                              QStringLiteral("a = 1;"),
                              QStringLiteral("b = 2;")};
    QMap<int,ReformatAnchor> anchors = anchorsOf(oldContent, {3});
    QStringList newContent = {QStringLiteral("a = 1;"),
                              QStringLiteral("b = 2;")};
    QMap<int,int> map = EditorAnchors::remapLines(anchors, newContent);
    QCOMPARE(map.value(3), 1);
}

void TestEditorAnchors::test_remap_reindentedContent()
{
    // a reformat mostly changes the indentation, which the keys ignore
    QStringList oldContent = {QStringLiteral("void f() {"),
                              QStringLiteral("a();"),
                              QStringLiteral("}")};
    QMap<int,ReformatAnchor> anchors = anchorsOf(oldContent, {1});
    QStringList newContent = {QStringLiteral("void f()"),
                              QStringLiteral("{"),
                              QStringLiteral("    a();"),
                              QStringLiteral("}")};
    QMap<int,int> map = EditorAnchors::remapLines(anchors, newContent);
    QCOMPARE(map.value(1), 2);
}

void TestEditorAnchors::test_remap_repeatedLinesUseTheirContext()
{
    // a lone "}" is too short to be matched on its own: its neighbours decide, so
    // the two closing braces of the file don't get mixed up
    QStringList oldContent = {QStringLiteral("void f() {"),
                              QStringLiteral("    a();"),
                              QStringLiteral("}"),
                              QStringLiteral("void g() {"),
                              QStringLiteral("    b();"),
                              QStringLiteral("}")};
    QMap<int,ReformatAnchor> anchors = anchorsOf(oldContent, {2, 5});
    QStringList newContent = {QStringLiteral("// x"),
                              QStringLiteral("// y"),
                              QStringLiteral("void f() {"),
                              QStringLiteral("    a();"),
                              QStringLiteral("}"),
                              QStringLiteral("void g() {"),
                              QStringLiteral("    b();"),
                              QStringLiteral("}")};
    QMap<int,int> map = EditorAnchors::remapLines(anchors, newContent);
    QCOMPARE(map.value(2), 4);
    QCOMPARE(map.value(5), 7);
}

void TestEditorAnchors::test_remap_anchorNotFoundKeepsTheOldLine()
{
    QStringList oldContent = {QStringLiteral("a = 1;"),
                              QStringLiteral("b = 2;"),
                              QStringLiteral("c = 3;")};
    QMap<int,ReformatAnchor> anchors = anchorsOf(oldContent, {1});
    QStringList newContent = {QStringLiteral("zzz"),
                              QStringLiteral("yyy")};
    QMap<int,int> map = EditorAnchors::remapLines(anchors, newContent);
    // the code is gone: the line keeps its old number instead of being dropped
    QCOMPARE(map.value(1), 1);
}

void TestEditorAnchors::test_remap_anchorPastTheEndOfTheContent()
{
    QStringList oldContent;
    for (int i=0;i<10;i++)
        oldContent.append(QStringLiteral("line %1;").arg(i));
    QMap<int,ReformatAnchor> anchors = anchorsOf(oldContent, {9});
    QStringList newContent = {QStringLiteral("other"), QStringLiteral("thing")};
    QMap<int,int> map = EditorAnchors::remapLines(anchors, newContent);
    // clamped to the last line of the new content
    QCOMPARE(map.value(9), 1);
}

void TestEditorAnchors::test_remap_emptyContent()
{
    QStringList oldContent = {QStringLiteral("a = 1;")};
    QMap<int,ReformatAnchor> anchors = anchorsOf(oldContent, {0});
    QMap<int,int> map = EditorAnchors::remapLines(anchors, QStringList());
    QCOMPARE(map.value(0), 0);
}

void TestEditorAnchors::test_remap_noAnchors()
{
    QStringList content = {QStringLiteral("a = 1;")};
    QVERIFY(EditorAnchors::remapLines(QMap<int,ReformatAnchor>(), content).isEmpty());
}
