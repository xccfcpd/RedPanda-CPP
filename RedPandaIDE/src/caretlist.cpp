/*
 * Copyright (C) 2020-2022 Roy Qu (royqh1979@gmail.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "caretlist.h"
#include <QDebug>
#include <QSet>

const PEditorCaret CaretList::NullCaret;

CaretList::CaretList(QObject* parent):
    QObject(parent),
    mIndex(-1),
    mPauseAdd(false)
{

}

void CaretList::addCaret(Editor *editor, int line, int aChar)
{
    if (mPauseAdd)
        return;
    for (int i=mList.size()-1;i>mIndex;i--) {
        removeCaret(i);
    }
    PEditorCaret caret = std::make_unique<EditorCaret>();
    caret->editor = editor;
    caret->line = line;
    caret->aChar = aChar;
    mList.push_back(std::move(caret));
    mIndex++;
    //qDebug()<<"add caret:"<<mIndex<<":"<<mList.count();
}

bool CaretList::hasPrevious() const
{
    //qDebug()<<"has previous:"<<mIndex<<":"<<mList.count();
    return mIndex>0;
}

bool CaretList::hasNext() const
{
    //qDebug()<<"has next:"<<mIndex<<":"<<mList.count();
    return mIndex<(int)mList.size()-1;
}

const PEditorCaret &CaretList::gotoAndGetPrevious()
{
    if (!hasPrevious())
        return NullCaret;
    mIndex--;
    //qDebug()<<"move previous:"<<mIndex<<":"<<mList.count();
    if (mIndex<(int)mList.size())
        return mList[mIndex];
    return NullCaret;
}

const PEditorCaret &CaretList::gotoAndGetNext()
{
    if (!hasNext())
        return NullCaret;
    mIndex++;
    //qDebug()<<"move next:"<<mIndex<<":"<<mList.count();
    if (mIndex>=0)
        return mList[mIndex];
    return NullCaret;
}

void CaretList::removeEditor(const Editor *editor)
{
    for (int i = mList.size()-1;i>=0;i--) {
        if (mList[i]->editor == editor)
            removeCaret(i);
    }
    mMergedCarets.remove(editor);
}

void CaretList::reset()
{
    mList.clear();
    mMergedCarets.clear();
    mIndex = -1;
}

void CaretList::pause()
{
    mPauseAdd = true;
}

void CaretList::unPause()
{
    mPauseAdd = false;
}

void CaretList::onLinesDeleted(const Editor *editor, int firstLine, int count)
{
    //qDebug()<<"deleted:"<<mIndex<<":"<<mList.count();
    for (int i=mList.size()-1;i>=0;i--) {
        if (mList[i]->editor == editor
                && mList[i]->line>=firstLine) {
            if (mList[i]->line < (firstLine+count))
                removeCaret(i);
            else
                mList[i]->line-=count;
        }
    }
}

void CaretList::onLinesInserted(const Editor *editor, int firstLine, int count)
{
    //qDebug()<<"inserted:"<<mIndex<<":"<<mList.count();
    for(PEditorCaret& caret:mList) {
        if (caret->editor == editor
                && caret->line >= firstLine)
            caret->line+=count;
    }
}

void CaretList::onLinesMoved(const Editor *editor, int fromLine, int toLine)
{
    for(PEditorCaret& caret:mList) {
        if (caret->editor == editor) {
            if (caret->line == fromLine)
                caret->line = toLine;
            else if (fromLine < toLine ) {
                if (fromLine < caret->line && caret->line <= toLine)
                    --caret->line;
            } else if (toLine < fromLine) {
                if (toLine <= caret->line && caret->line <= fromLine)
                    caret->line++;
            }
        }
    }
}

// Puts the carets of a file back on their code after the whole content of its
// editor has been replaced: "lineMap" maps the lines of the old content to the
// lines of the new one (see Editor::replaceContentAndRemap()). The incremental
// line bookkeeping (onLinesDeleted() and the others) is skipped while that
// replacement runs, so this is the only place that moves them.
void CaretList::remapLines(const Editor *editor, const QMap<int,int> &lineMap)
{
    // The whole content is replaced: the merge records describe lines of a version
    // that doesn't exist any more.
    mMergedCarets.remove(editor);
    for (PEditorCaret& caret:mList) {
        if (caret->editor!=editor)
            continue;
        auto it=lineMap.constFind(caret->line);
        if (it!=lineMap.constEnd())
            caret->line=it.value();
    }
}

void CaretList::onLinesMerged(const Editor *editor, int removedLine, int intoLine)
{
    QList<EditorCaret*> moved;
    for (PEditorCaret& caret:mList) {
        if (caret->editor==editor && caret->line==removedLine) {
            // the code of the removed line is appended to the line it is merged into
            caret->line = intoLine;
            moved.append(caret.get());
        }
    }
    if (!moved.isEmpty())
        mMergedCarets[editor].append(MergedCarets{removedLine, moved});
}

void CaretList::onLinesSplit(const Editor *editor, int mergedLine, int newLine)
{
    auto it = mMergedCarets.find(editor);
    if (it==mMergedCarets.end())
        return;
    QList<MergedCarets>& merges = it.value();
    // The split that undoes the merge which removed `newLine` is the innermost one
    // that did (merges are undone in the reverse order).
    int mergeIndex = -1;
    for (int i=merges.count()-1;i>=0;i--) {
        if (merges[i].removedLine==newLine) {
            mergeIndex = i;
            break;
        }
    }
    if (mergeIndex<0)
        return;
    QList<EditorCaret*> moved = merges.takeAt(mergeIndex).carets;
    for (EditorCaret* caret: moved) {
        if (caret->line >= mergedLine)
            caret->line = newLine;
    }
    if (merges.isEmpty())
        mMergedCarets.erase(it);
}

QList<int> CaretList::caretLines(const Editor *editor) const
{
    // One anchor per entry would be far too many: the same line is visited over
    // and over while editing, and one anchor per distinct line is enough to move
    // them all (remapLines() moves the entries themselves).
    QSet<int> lines;
    for (const PEditorCaret& caret:mList) {
        if (caret->editor==editor)
            lines.insert(caret->line);
    }
    return lines.values();
}

void CaretList::removeCaret(int index)
{
    if (index<0 || index>=(int)mList.size())
        return;
    forgetCaret(mList[index].get());
    mList.erase(mList.begin()+index);
    if (mIndex>=index)
        mIndex--;
}

void CaretList::forgetCaret(const EditorCaret *caret)
{
    // The entry is about to be destroyed: no merge record may keep its address.
    for (auto it=mMergedCarets.begin();it!=mMergedCarets.end();) {
        QList<MergedCarets>& merges = it.value();
        for (int i=merges.count()-1;i>=0;i--) {
            merges[i].carets.removeAll(const_cast<EditorCaret*>(caret));
            if (merges[i].carets.isEmpty())
                merges.removeAt(i);
        }
        if (merges.isEmpty())
            it = mMergedCarets.erase(it);
        else
            ++it;
    }
}
