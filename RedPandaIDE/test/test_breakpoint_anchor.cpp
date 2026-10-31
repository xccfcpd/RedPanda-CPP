#include "test_breakpoint_anchor.h"

#include "src/debugger/breakpointanchor.h"

#include <QElapsedTimer>
#include <QTest>

TestBreakpointAnchor::TestBreakpointAnchor(QObject *parent):
    QObject{parent}
{
}

// The neighbours of a line are part of its anchor: two identical lines that don't
// share their neighbours are told apart.
void TestBreakpointAnchor::test_contextTellsRepeatedLinesApart()
{
    QStringList content({"int a;","return 0;","}","int b;","return 0;","}"});
    QString first = contentContextFingerprint(content,1);
    QString second = contentContextFingerprint(content,4);
    QVERIFY(first!=second);
    QCOMPARE(findNearestLineByAnchor(content,second,1,true),4);
    QCOMPARE(findNearestLineByAnchor(content,first,4,true),1);
}

// When the same anchor is found on several lines, the one closest to the line the
// breakpoint was on wins.
void TestBreakpointAnchor::test_nearestLineWins()
{
    QStringList content({"a;","return 0;","}","a;","return 0;","}"});
    QString anchor = contentContextFingerprint(content,1);
    QCOMPARE(contentContextFingerprint(content,4),anchor);
    QCOMPARE(findNearestLineByAnchor(content,anchor,0,true),1);
    QCOMPARE(findNearestLineByAnchor(content,anchor,5,true),4);
}

// A plain anchor of one character (a lone "}") matches too many places to be
// relied on, so it is never searched for.
void TestBreakpointAnchor::test_plainAnchorOfOneCharIsNeverUsed()
{
    QStringList content({"a;","}","b;","}"});
    QString lone = breakpointFingerprint(QStringLiteral("}"));
    QCOMPARE(lone.length(),1);
    QCOMPARE(findNearestLineByAnchor(content,lone,0,false),-1);
}

// "uniqueOnly" refuses an anchor that matches more than one line, so that a plain
// line anchor cannot move a breakpoint to another line holding the same code.
void TestBreakpointAnchor::test_uniqueOnlyRejectsAnAmbiguousAnchor()
{
    QStringList content({"a;","return 0;","}","a;","return 0;","}"});
    QString plain = breakpointFingerprint(QStringLiteral("return 0;"));
    QCOMPARE(findNearestLineByAnchor(content,plain,0,false),1);
    QCOMPARE(findNearestLineByAnchor(content,plain,0,false,true),-1);
}

// The whole point: the breakpoint follows its code when a line is inserted before
// it.
void TestBreakpointAnchor::test_reanchor_followsTheCode()
{
    QStringList before({"int main()","{","    int x = 0;","    return x;","}"});
    QString anchor = contentContextFingerprint(before,3);
    QStringList after({"// a new line","int main()","{","    int x = 0;","    return x;","}"});
    ReanchoredBreakpoint result = reanchorBreakpoint(after,3,anchor);
    QCOMPARE(result.line,4);
    QVERIFY(result.changed);
    QCOMPARE(result.fingerprint,contentContextFingerprint(after,4));
}

// A breakpoint loaded from a config file written before the anchors existed has
// no fingerprint: it stays where it is and remembers its code from now on.
void TestBreakpointAnchor::test_reanchor_remembersTheAnchorOfAnOlderBreakpoint()
{
    QStringList content({"int a;","int b;"});
    ReanchoredBreakpoint result = reanchorBreakpoint(content,1,QString());
    QCOMPARE(result.line,1);
    QVERIFY(result.changed);
    QCOMPARE(result.fingerprint,contentContextFingerprint(content,1));
}

// The code the breakpoint was set on is not there any more (and no line holds it
// elsewhere): the breakpoint is never dropped, it stays on its line and keeps its
// anchor - the only memory of the code it was set on.
void TestBreakpointAnchor::test_reanchor_keepsTheLineWhenTheCodeIsGone()
{
    QStringList content({"int a;","int b;"});
    QString gone = breakpointContextFingerprint(QStringLiteral("zzz();"),
                                                QStringLiteral("yyy();"),
                                                QStringLiteral("xxx();"));
    ReanchoredBreakpoint result = reanchorBreakpoint(content,1,gone);
    QCOMPARE(result.line,1);
    QCOMPARE(result.fingerprint,gone);
    // nothing changed: the model isn't told, the breakpoint isn't rewritten
    QVERIFY(!result.changed);
}

// The line the breakpoint was on has just been merged with the line above: the
// code is still in the file, but inside a line holding more than it did, so no
// line anchors it. The breakpoint moved onto that merged line (the editor tells
// the models about the merge) and must keep its anchor: that is what will put it
// back on its code.
void TestBreakpointAnchor::test_reanchor_keepsTheAnchorWhileTheCodeIsMerged()
{
    QStringList before({"int a;","keepMe();","int b;"});
    QString anchor = contentContextFingerprint(before,1);
    QStringList merged({"int a;keepMe();","int b;"});
    ReanchoredBreakpoint result = reanchorBreakpoint(merged,0,anchor);
    QCOMPARE(result.line,0);
    QCOMPARE(result.fingerprint,anchor);
    QVERIFY(!result.changed);
}

// The same thing, but the line holding the code is not the one the breakpoint is on
// any more (the merge moved it a line above): the line is followed through
// findLineHoldingMostOfTheCode(), the anchor is not touched. Rewriting it there would
// store the text of a line holding a piece of two lines - no line will ever hold that
// again, so the breakpoint would lose the code for good and drift line by line as the
// following edits (Ctrl+Z after Ctrl+Z) come in.
void TestBreakpointAnchor::test_reanchor_keepsTheAnchorWhenTheCodeIsInsideALongerLine()
{
    QStringList before({"int a;","keepMe();","int b;"});
    QString anchor = contentContextFingerprint(before,1);
    // the lines were joined: "keepMe();" now sits inside the first line, together with
    // the code of the lines around it
    QStringList merged({"int a;keepMe();int b;","int c;","int d;"});
    ReanchoredBreakpoint result = reanchorBreakpoint(merged,2,anchor);
    QCOMPARE(result.line,0);
    QCOMPARE(result.fingerprint,anchor);
    QVERIFY(result.changed);
    // undoing the edit puts the code back on a line of its own: the anchor kept above
    // recognizes it, and the breakpoint goes back onto it
    ReanchoredBreakpoint undone = reanchorBreakpoint(before,result.line,
                                                     result.fingerprint);
    QCOMPARE(undone.line,1);
}

// Undoing that merge gives the line, and its code, back: the anchor kept above
// finds it again, and the breakpoint goes back onto it.
void TestBreakpointAnchor::test_reanchor_findsTheCodeAgainWhenTheMergeIsUndone()
{
    QStringList before({"int a;","keepMe();","int b;"});
    QString anchor = contentContextFingerprint(before,1);
    QStringList merged({"int a;keepMe();","int b;"});
    // the breakpoint is on the merged line, with the anchor it was set with
    ReanchoredBreakpoint mergedResult = reanchorBreakpoint(merged,0,anchor);
    QCOMPARE(mergedResult.fingerprint,anchor);
    // Ctrl+Z ...
    ReanchoredBreakpoint undone = reanchorBreakpoint(before,mergedResult.line,
                                                     mergedResult.fingerprint);
    QCOMPARE(undone.line,1);
    QVERIFY(undone.changed);
    QCOMPARE(undone.fingerprint,anchor);
}

// Several breakpoints in the same file: the ones of the lines a merge joins follow
// their code, and each one comes back on its own line when the merge is undone.
void TestBreakpointAnchor::test_reanchor_restoresSeveralBreakpointsAfterAMergeAndItsUndo()
{
    QStringList before({"int a(){","return 0;","}","int b(){","return 0;","}"});
    QString first = contentContextFingerprint(before,1);
    QString second = contentContextFingerprint(before,4);
    // the line "}" and the line "int b(){" are joined: the breakpoint of line 1 stays
    // on its code, the one of line 4 is now on line 3, with its anchor kept
    QStringList merged({"int a(){","return 0;","}int b(){","return 0;","}"});
    ReanchoredBreakpoint firstAfterMerge = reanchorBreakpoint(merged,1,first);
    QCOMPARE(firstAfterMerge.line,1);
    QCOMPARE(firstAfterMerge.fingerprint,first);
    ReanchoredBreakpoint secondAfterMerge = reanchorBreakpoint(merged,3,second);
    QCOMPARE(secondAfterMerge.line,3);
    QCOMPARE(secondAfterMerge.fingerprint,second);
    // Ctrl+Z: the merge is undone, and each breakpoint finds its own code again
    ReanchoredBreakpoint firstUndone = reanchorBreakpoint(before,firstAfterMerge.line,
                                                          firstAfterMerge.fingerprint);
    QCOMPARE(firstUndone.line,1);
    QCOMPARE(firstUndone.fingerprint,first);
    ReanchoredBreakpoint secondUndone = reanchorBreakpoint(before,secondAfterMerge.line,
                                                           secondAfterMerge.fingerprint);
    QCOMPARE(secondUndone.line,4);
    QVERIFY(secondUndone.changed);
    QCOMPARE(secondUndone.fingerprint,contentContextFingerprint(before,4));
}

// An edit split the line the code is on: the code is now on two lines, and only a
// piece of it is on the one the breakpoint was left on. Storing the anchor of that
// piece would forget the rest of the code the breakpoint was set on - and a piece is
// too short to be recognized once the split is undone, so the breakpoint would be
// stuck there. The anchor is kept instead: it is what puts the breakpoint back on its
// own line when the code is one line again.
void TestBreakpointAnchor::test_reanchor_keepsTheAnchorWhenTheLineWasSplit()
{
    QStringList before({"for (j = 1; j <= i; j++) {",
                        "cout << i << \"*\" << j;cout << \"\\n\";",
                        "}"});
    QString anchor = contentContextFingerprint(before,1);
    // the line is split in two, and the breakpoint is left on the first piece
    QStringList split({"for (j = 1; j <= i; j++) {",
                       "cout << i << \"*\" << j;",
                       "cout << \"\\n\";",
                       "}"});
    ReanchoredBreakpoint result = reanchorBreakpoint(split,2,anchor);
    QCOMPARE(result.line,1);
    QVERIFY(result.changed);
    QCOMPARE(result.fingerprint,anchor);
    // undoing the edit puts the code back on one line: the kept anchor finds it
    ReanchoredBreakpoint undone = reanchorBreakpoint(before,result.line,
                                                     result.fingerprint);
    QCOMPARE(undone.line,1);
    QCOMPARE(undone.fingerprint,anchor);
}

// Nothing moved: the anchor is still the one of the line, so nothing is reported
// to the model (the breakpoint is not touched).
void TestBreakpointAnchor::test_reanchor_doesNothingWhenNothingMoved()
{
    QStringList content({"int a;","int b;","int c;"});
    QString anchor = contentContextFingerprint(content,1);
    ReanchoredBreakpoint result = reanchorBreakpoint(content,1,anchor);
    QCOMPARE(result.line,1);
    QVERIFY(!result.changed);
    QCOMPARE(result.fingerprint,anchor);
}

// A fingerprint stored before the neighbours were kept (a plain line fingerprint,
// with no newline in it) is still usable.
void TestBreakpointAnchor::test_reanchor_readsAPlainFingerprint()
{
    QStringList content({"x;","uniqueCall();","y;"});
    QString plain = breakpointFingerprint(QStringLiteral("uniqueCall();"));
    QVERIFY(!plain.contains(QLatin1Char('\n')));
    ReanchoredBreakpoint result = reanchorBreakpoint(content,0,plain);
    QCOMPARE(result.line,1);
    QVERIFY(result.changed);
}

// Guard against a complexity blow-up: re-anchoring scans the whole content once
// per breakpoint and runs on the GUI thread when a file is reloaded or
// reformatted. 20 breakpoints on a 20000 line file must stay well under a second.
void TestBreakpointAnchor::test_reanchor_scalesToALargeContent()
{
    QStringList content;
    content.reserve(20000);
    for (int i=0;i<20000;i++)
        content.append(QStringLiteral("int value%1 = %1;").arg(i));
    // the anchor of the last line, and a breakpoint that was on the first one:
    // the code is looked for across the whole content
    QString anchor = contentContextFingerprint(content,19999);
    QElapsedTimer timer;
    timer.start();
    int found = 0;
    for (int i=0;i<20;i++) {
        ReanchoredBreakpoint result = reanchorBreakpoint(content,0,anchor);
        if (result.line==19999)
            ++found;
    }
    const qint64 elapsed = timer.elapsed();
    qInfo() << "20 re-anchorings of a 20000 line file:" << elapsed << "ms";
    QCOMPARE(found,20);
    QVERIFY2(elapsed<5000,qPrintable(QStringLiteral("took %1 ms").arg(elapsed)));
}

// The code is merged into another line by an edit, and the file is reformatted
// before the merge is undone (astyle replaces the whole content, see
// Editor::finishReformat()). The code is on a line of its own again, but the line
// doesn't hold what it held before (the reformat rewrote its end and put more code
// in front of it), so its anchor finds nothing - and the breakpoint would stay on
// the line number the merge left it on, in the middle of other code. It has to
// follow the code, which is the only line holding most of it.
void TestBreakpointAnchor::test_reanchor_findsTheCodeAfterAMergeAndAReformat()
{
    QStringList readable({
        "#include <iostream>",
        "using namespace std;",
        "int main() {",
        "    int i, j;",
        "    for (i = 1; i < 10; i++) {",
        "        for (j = 1; j <= i; j++) {{",
        "            cout << i << \"*\" << j << \"=\" << i*j << \" \";}}}",
        "            cout << \"\\n\";",
        "    }",
        "}"});
    QString anchor = contentContextFingerprint(readable,6);
    // everything from "int i, j;" on was joined into the line of "main()"
    QStringList merged({
        "#include <iostream>",
        "using namespace std;",
        "int main() {",
        "    int i, j;for (i = 1; i < 10; i++) {for (j = 1; j <= i; j++) {{} "
        "cout << i << \"*\" << j << \"=\" << i*j << \" \";}}}",
        "}"});
    // the merge moved the breakpoint onto the merged line, keeping its anchor
    ReanchoredBreakpoint afterMerge = reanchorBreakpoint(merged,3,anchor);
    QCOMPARE(afterMerge.line,3);
    QCOMPARE(afterMerge.fingerprint,anchor);
    QVERIFY(!afterMerge.changed);
    // the file is reformatted: the merged line is split back and its end rewritten
    QStringList formatted({
        "#include <iostream>",
        "using namespace std;",
        "int main() {",
        "    int i, j;",
        "    for (i = 1; i < 10; i++) {",
        "        for (j = 1; j <= i; j++) {",
        "            {} cout << i << \"*\" << j << \"=\" << i*j << \" \",",
        "        }",
        "    }",
        "}",
        "} cout << \"\\n\";",
        "}",
        "}"});
    ReanchoredBreakpoint afterReformat = reanchorBreakpoint(formatted,afterMerge.line,
                                                            afterMerge.fingerprint);
    QCOMPARE(afterReformat.line,6);
    QVERIFY(afterReformat.changed);
    QCOMPARE(afterReformat.fingerprint,contentContextFingerprint(formatted,6));
}

// A breakpoint whose code has gone from the file keeps the line it was on (see
// test_reanchor_keepsTheLineWhenTheCodeIsGone()), and that line can be past the end of
// the content. Debugger::sendAllBreakpointsToDebugger() asks this before it gives a
// breakpoint to the debugger, and leaves the breakpoints of a file it knows nothing
// about (a lineCount of 0: the file can't be read) alone.
void TestBreakpointAnchor::test_lineIsTheLineOfTheCode_rejectsMergedPrefixAndSuffix()
{
    const QString fingerprint = QStringLiteral("prev\nintx;\nnext");
    QVERIFY(lineIsTheLineOfTheCode(QStringLiteral("intx;"), fingerprint));
    // A continuation line joined to the following line.
    QVERIFY(!lineIsTheLineOfTheCode(QStringLiteral("intx;inty;"),
                                    QStringLiteral("prev\nintx;\ninty;")));
    // A line joined to the previous line.
    QVERIFY(!lineIsTheLineOfTheCode(QStringLiteral("inty;intx;"),
                                    QStringLiteral("inty;\nintx;\nnext")));
    // A formatter adding a prefix does not make the line a continuation line:
    // the anchor may be refreshed.
    QVERIFY(lineIsTheLineOfTheCode(QStringLiteral("elseintx;"), fingerprint));
}

void TestBreakpointAnchor::test_lineIsInContent_rejectsALinePastTheEnd()
{
    QStringList content({"#include <iostream>","int main() {","}"});
    for (int line=0;line<content.count();line++)
        QVERIFY(lineIsInContent(line,content.count()));
    // the code the breakpoint was set on was deleted from the file, and the breakpoint
    // kept its line: it is past the end of the content now
    QVERIFY(!lineIsInContent(100,content.count()));
    QVERIFY(!lineIsInContent(content.count(),content.count()));
}

// A very short line (shorter than the "most of the code" threshold) can be joined
// into the middle of a longer line when a formatter merges several lines. The
// breakpoint must follow it there, not stay on its old line number - that line can
// even be past the end of the formatted content.
void TestBreakpointAnchor::test_reanchor_findsAShortLineInsideALongerLine()
{
    QStringList before({"p();","q();","x();","b();","c();"});
    QString anchor = contentContextFingerprint(before,2);
    QStringList merged({"p();","q();x();b();","c();"});
    ReanchoredBreakpoint result = reanchorBreakpoint(merged,2,anchor);
    QCOMPARE(result.line,1);
    QCOMPARE(result.fingerprint,anchor);
    QVERIFY(result.changed);
    // undoing the format puts the line back on its own line
    ReanchoredBreakpoint undone = reanchorBreakpoint(before,result.line,
                                                     result.fingerprint);
    QCOMPARE(undone.line,2);
}

// A formatter can attach a short line to the line above it ("} else {"): the code is
// still there, inside the longer line, and the breakpoint follows it instead of being
// left on the old line number.
void TestBreakpointAnchor::test_reanchor_findsAShortLineMergedIntoAnother()
{
    QStringList before({"if (a) {","x();","}","else {","y();","}"});
    QString anchor = contentContextFingerprint(before,3);
    QStringList formatted({"if (a) {","x();","} else {","y();","}"});
    ReanchoredBreakpoint result = reanchorBreakpoint(formatted,3,anchor);
    QCOMPARE(result.line,2);
    QCOMPARE(result.fingerprint,anchor);
    QVERIFY(result.changed);
}
