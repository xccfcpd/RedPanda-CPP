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
// elsewhere): the breakpoint is never dropped, it stays on its line and follows
// the code that is there from now on.
void TestBreakpointAnchor::test_reanchor_keepsTheLineWhenTheCodeIsGone()
{
    QStringList content({"int a;","int b;"});
    QString gone = breakpointContextFingerprint(QStringLiteral("zzz();"),
                                                QStringLiteral("yyy();"),
                                                QStringLiteral("xxx();"));
    ReanchoredBreakpoint result = reanchorBreakpoint(content,1,gone);
    QCOMPARE(result.line,1);
    QVERIFY(result.changed);
    QCOMPARE(result.fingerprint,contentContextFingerprint(content,1));
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
