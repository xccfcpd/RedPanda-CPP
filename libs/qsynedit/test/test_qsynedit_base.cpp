#include <QTest>
#include <QCoreApplication>

#include "test_qsynedit_base.h"
#include "qsynedit/qsynedit.h"
#include "qsynedit/document.h"
#include "qsynedit/syntaxer/cpp.h"
#include "qsynedit/formatter/cppformatter.h"

namespace QSynedit {

void TestQSyneditBase::onLinesDeleted(int line, int count)
{
    mLineSignals.append(DeletedLine);
    mDeleteStartLines.append(line);
    mDeleteLineCounts.append(count);
}

void TestQSyneditBase::onLinesInserted(int line, int count)
{
    mLineSignals.append(InsertedLine);
    mInsertStartLines.append(line);
    mInsertLineCounts.append(count);
}

void TestQSyneditBase::onLineMoved(int from, int to)
{
    mLineSignals.append(MovedLine);
    mLineMovedFroms.append(from);
    mLineMovedTos.append(to);
}

void TestQSyneditBase::onLinesMerged(int removedLine, int intoLine)
{
    mLineSignals.append(MergedLine);
    mMergedLines.append(removedLine);
    mMergedIntoLines.append(intoLine);
}

void TestQSyneditBase::onLinesSplit(int mergedLine, int newLine)
{
    mLineSignals.append(SplitLine);
    mSplitLines.append(mergedLine);
    mSplitNewLines.append(newLine);
    mSplitContent = mEdit->content();
}

void TestQSyneditBase::onStatusChanged(StatusChanges change)
{
    mStatusChanges.append(change & ~(StatusChange::TopPos) & ~(StatusChange::LeftPos));
}

void TestQSyneditBase::onReparsed(int start, int count)
{
    mReparseStarts.append(start);
    mReparseCounts.append(count);
}

void TestQSyneditBase::clearReparseDatas()
{
    mReparseStarts.clear();
    mReparseCounts.clear();
}

void TestQSyneditBase::clearSignalDatas()
{
    mInsertLineCounts.clear();
    mInsertStartLines.clear();
    mDeleteLineCounts.clear();
    mDeleteStartLines.clear();
    mLineMovedFroms.clear();
    mLineMovedTos.clear();
    mMergedLines.clear();
    mMergedIntoLines.clear();
    mSplitLines.clear();
    mSplitNewLines.clear();
    mSplitContent.clear();
    mLineSignals.clear();
    mStatusChanges.clear();
    mReparseStarts.clear();
    mReparseCounts.clear();
}

void TestQSyneditBase::clearContent()
{
    mEdit->processCommand(EditCommand::ClearAll);
    clearSignalDatas();
}

void TestQSyneditBase::connectEditSignals()
{
    connect(mEdit.get(), &QSynEdit::linesDeleted, this, &TestQSyneditBase::onLinesDeleted);
    connect(mEdit.get(), &QSynEdit::linesInserted, this, &TestQSyneditBase::onLinesInserted);
    connect(mEdit.get(), &QSynEdit::lineMoved, this, &TestQSyneditBase::onLineMoved);
    connect(mEdit.get(), &QSynEdit::linesMerged, this, &TestQSyneditBase::onLinesMerged);
    connect(mEdit.get(), &QSynEdit::linesSplit, this, &TestQSyneditBase::onLinesSplit);
    connect(mEdit.get(), &QSynEdit::statusChanged, this, &TestQSyneditBase::onStatusChanged);
    connect(mEdit.get(), &QSynEdit::linesReparesd, this, &TestQSyneditBase::onReparsed);
}

}

