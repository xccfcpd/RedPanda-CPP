#ifndef TEST_QSYNEDIT_BASE_H
#define TEST_QSYNEDIT_BASE_H
#include <QTest>
#include "qsynedit/qsynedit.h"

template <>inline char *QTest::toString(const QSynedit::CharPos &pos) {
    return toString(QString("CharPos(ch=%1,line=%2)").arg(pos.ch).arg(pos.line));
}

namespace QSynedit{

class TestQSyneditBase : public QObject
{
    Q_OBJECT
protected:
    std::shared_ptr<QSynEdit> mEdit;
    QList<int> mDeleteStartLines;
    QList<int> mDeleteLineCounts;
    QList<int> mInsertStartLines;
    QList<int> mInsertLineCounts;
    QList<int> mLineMovedFroms;
    QList<int> mLineMovedTos;
    QList<int> mMergedLines;
    QList<int> mMergedIntoLines;
    QList<int> mSplitLines;
    QList<int> mSplitNewLines;
    // The content the editor has when linesSplit() is sent: the application reads it
    // from the editor to put the markers back on their code, so the line must already
    // be split at that moment.
    QStringList mSplitContent;
    // The order the line signals are sent in matters: when the lines are merged the
    // application must be told that the markers of the line that disappears follow
    // their code (linesMerged()) *before* the line is reported as deleted.
    enum LineSignal { DeletedLine, InsertedLine, MovedLine, MergedLine, SplitLine };
    QList<LineSignal> mLineSignals;
    QList<int> mStatusChanges;
    QList<int> mReparseStarts;
    QList<int> mReparseCounts;
protected:
    void clearReparseDatas();
    void clearSignalDatas();
    void clearContent();
    void connectEditSignals();
protected slots:
    void onLinesDeleted(int line, int count);
    void onLinesInserted(int line, int count);
    void onLineMoved(int from, int to);
    void onLinesMerged(int removedLine, int intoLine);
    void onLinesSplit(int mergedLine, int newLine);
    void onStatusChanged(StatusChanges change);
    void onReparsed(int start, int count);
};

}
#endif
