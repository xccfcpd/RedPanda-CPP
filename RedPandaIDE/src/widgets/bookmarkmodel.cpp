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
#include "bookmarkmodel.h"
#include "../systemconsts.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>
#include <QDateTime>
#include <qt_utils/utils.h>

BookmarkModel::BookmarkModel(QObject* parent):QAbstractTableModel(parent),
    mIsForProject(false)
{

}

QSet<int> BookmarkModel::bookmarksInFile(const QString &filename, bool forProject)
{
    QSet<int> lines;
    QList<PBookmark> bookmarks;
    if (forProject)
        bookmarks = mProjectBookmarks;
    else
        bookmarks = mBookmarks;
    foreach (const PBookmark& bookmark, bookmarks) {
        if (bookmark->filename.compare(filename, PATH_SENSITIVITY) == 0) {
            lines.insert(bookmark->line);
        }
    }
    return lines;
}

void BookmarkModel::addBookmark(const QString &filename, int line, const QString &description, bool forProject)
{
    // A line can hold several bookmarks with different descriptions.
    PBookmark bookmark = std::make_shared<Bookmark>();
    bookmark->filename = filename;
    bookmark->line = line;
    bookmark->description = description;
    bookmark->timestamp = QDateTime::currentMSecsSinceEpoch();
    if (forProject) {
        if (forProject==mIsForProject)
            beginInsertRows(QModelIndex(),mProjectBookmarks.count(),mProjectBookmarks.count());
        mProjectBookmarks.append(bookmark);
    } else {
        if (forProject==mIsForProject)
            beginInsertRows(QModelIndex(),mBookmarks.count(),mBookmarks.count());
        mBookmarks.append(bookmark);
    }
    if (forProject==mIsForProject)
        endInsertRows();
}

PBookmark BookmarkModel::bookmark(int i, bool forProject)
{
    if (forProject)
        return mProjectBookmarks[i];
    else
        return mBookmarks[i];
}

PBookmark BookmarkModel::bookmark(int i)
{
    return bookmark(i,isForProject());
}

PBookmark BookmarkModel::bookmark(const QString &filename, int line, bool forProject)
{
    QList<PBookmark> bookmarks;
    if (forProject)
        bookmarks = mProjectBookmarks;
    else
        bookmarks = mBookmarks;
    foreach (PBookmark bookmark, bookmarks) {
        if (bookmark->filename.compare(filename, PATH_SENSITIVITY) == 0
                && bookmark->line == line) {
            return bookmark;
        }
    }
    return PBookmark();
}

PBookmark BookmarkModel::bookmark(const QString &filename, int line)
{
    return bookmark(filename,line,isForProject());
}

QList<PBookmark> BookmarkModel::bookmarksAt(const QString &filename, int line,
                                            bool forProject) const
{
    const QList<PBookmark>& bookmarks = forProject ? mProjectBookmarks : mBookmarks;
    QList<PBookmark> result;
    for (const PBookmark& bookmark: bookmarks) {
        if (bookmark->filename.compare(filename, PATH_SENSITIVITY) == 0
                && bookmark->line == line)
            result.append(bookmark);
    }
    return result;
}

int BookmarkModel::rowOfBookmark(const PBookmark &bookmark, bool forProject) const
{
    const QList<PBookmark>& bookmarks = forProject ? mProjectBookmarks : mBookmarks;
    for (int i=0;i<bookmarks.count();i++) {
        if (bookmarks[i]==bookmark)
            return i;
    }
    return -1;
}

bool BookmarkModel::removeBookmark(const QString &filename, int line, bool forProject)
{
    QList<PBookmark> bookmarks;
    if (forProject)
        bookmarks = mProjectBookmarks;
    else
        bookmarks = mBookmarks;
    for (int i=0;i<bookmarks.count();i++) {
        PBookmark bookmark = bookmarks[i];
        if (bookmark->filename.compare(filename, PATH_SENSITIVITY) == 0
                && bookmark->line == line) {
            removeBookmarkAt(i, forProject);
            return true;
        }
    }
    return false;
}

void BookmarkModel::removeBookmarks(const QString &filename, bool forProject)
{
    // What was remembered of the merges of the file (and of the splits that put its
    // bookmarks back on their line) is about bookmarks that are dropped just below:
    // keeping it would leave records that only a split of another version of the file
    // could match, and the bookmarks they hold alive (see onFileSplitLines()).
    // Only the records of this model are dropped: the same file name can belong to the
    // project model and to the other one, and the bookmarks of the other one are still
    // there.
    auto mergedIt = mMergedBookmarks.find(filename);
    if (mergedIt != mMergedBookmarks.end()) {
        QList<MergedLines>& merges = mergedIt.value();
        for (int i=merges.count()-1;i>=0;i--) {
            if (merges[i].forProject == forProject)
                merges.removeAt(i);
        }
        if (merges.isEmpty())
            mMergedBookmarks.erase(mergedIt);
    }
    for (int i=mBookmarksPutBackBySplit.count()-1;i>=0;i--) {
        if (mBookmarksPutBackBySplit[i].forProject == forProject
                && mBookmarksPutBackBySplit[i].bookmark->filename.compare(filename, PATH_SENSITIVITY) == 0)
            mBookmarksPutBackBySplit.removeAt(i);
    }
    QList<PBookmark> bookmarks;
    if (forProject)
        bookmarks = mProjectBookmarks;
    else
        bookmarks = mBookmarks;
    for (int i=bookmarks.count()-1;i>=0;i--) {
        PBookmark bookmark = bookmarks[i];
        if (bookmark->filename.compare(filename, PATH_SENSITIVITY) == 0) {
            removeBookmarkAt(i, forProject);
        }
    }
}

void BookmarkModel::moveBookmarksInFile(const QString &filename, const QMap<int,int> &lineMap,
                                        int lineCount, bool forProject, bool contentRestoredByUndoRedo)
{
    // What was remembered of the merges of the file (see onFileMergeLines()) describes
    // the lines of a document version that is being replaced here: a split of the new
    // content that happens to be reported on one of those lines would only put the
    // bookmarks of that merge back on a wrong one. The undo/redo of a merge is the
    // exception: the split it runs is exactly what those bookmarks wait for, so the
    // records are kept while the content is restored by one. Only the records of this
    // model are dropped: the other model may hold a file with the same name.
    if (!contentRestoredByUndoRedo) {
        auto mergedIt = mMergedBookmarks.find(filename);
        if (mergedIt != mMergedBookmarks.end()) {
            QList<MergedLines>& merges = mergedIt.value();
            for (int i=merges.count()-1;i>=0;i--) {
                if (merges[i].forProject == forProject)
                    merges.removeAt(i);
            }
            if (merges.isEmpty())
                mMergedBookmarks.erase(mergedIt);
        }
    }
    QList<PBookmark> bookmarks;
    if (forProject)
        bookmarks = mProjectBookmarks;
    else
        bookmarks = mBookmarks;
    // The bookmarks that the merge being undone put back on their own line (see
    // onFileSplitLines()) are looked at first: their line is known exactly, while the
    // line map is keyed by the lines the old content had. The line one of them sits on
    // is the line another marker was only passing through while the merge lasted, and
    // its number maps to that one's code: moving the bookmark by the map would take it
    // away from the line the split put it back on.
    QList<PBookmark> putBack;
    for (int i=mBookmarksPutBackBySplit.count()-1;i>=0;i--) {
        const PutBackBookmark& entry = mBookmarksPutBackBySplit[i];
        if (entry.forProject == forProject
                && entry.bookmark->filename.compare(filename, PATH_SENSITIVITY) == 0) {
            putBack.append(entry.bookmark);
            mBookmarksPutBackBySplit.removeAt(i);
        }
    }
    // The model allows several bookmarks on one line now.  A line map can
    // therefore map several old lines to the same new line: they all keep that
    // line instead of being pushed to different free lines, which would move a
    // bookmark away from the code it was on.
    const int lastLine = qMax(0, lineCount-1);
    for (int i=0;i<bookmarks.count();i++) {
        PBookmark bookmark = bookmarks[i];
        if (bookmark->filename.compare(filename, PATH_SENSITIVITY) != 0)
            continue;
        if (putBack.contains(bookmark))
            continue;
        int newLine = lineMap.value(bookmark->line, bookmark->line);
        if (newLine==bookmark->line)
            continue;
        bookmark->line = qBound(0, newLine, lastLine);
        if (forProject==mIsForProject)
            emit dataChanged(createIndex(i,0),createIndex(i,2));
    }
}

void BookmarkModel::renameBookmarkFile(const QString& oldFilename, const QString& newFilename, bool forProject)
{
    // A file rename is not about one model: the same path can be held by the project
    // model and by the other one, and all their bookmarks (and the merge records that
    // point to them) must follow it. The active list is the one that gets the
    // dataChanged() notifications.
    Q_UNUSED(forProject);
    auto renameList = [this, &oldFilename, &newFilename](QList<PBookmark>& bookmarks,
                                                        bool listForProject) {
        for (int i=0;i<bookmarks.count();i++) {
            PBookmark bookmark = bookmarks[i];
            if (bookmark->filename.compare(oldFilename, PATH_SENSITIVITY) != 0)
                continue;
            bookmark->filename = newFilename;
            if (listForProject == mIsForProject) {
                QModelIndex index = createIndex(i,2);
                emit dataChanged(index,index);
            }
        }
    };
    renameList(mProjectBookmarks, true);
    renameList(mBookmarks, false);
    // The split/merge records point to the same bookmarks; update the ones whose
    // bookmark was removed from its list in between too, so that a later split can
    // still match them.
    for (PutBackBookmark& entry : mBookmarksPutBackBySplit) {
        if (entry.bookmark->filename.compare(oldFilename, PATH_SENSITIVITY) == 0)
            entry.bookmark->filename = newFilename;
    }
    for (auto it=mMergedBookmarks.begin();it!=mMergedBookmarks.end();++it) {
        for (MergedLines& merge : it.value()) {
            for (PBookmark& bookmark : merge.bookmarks) {
                if (bookmark->filename.compare(oldFilename, PATH_SENSITIVITY) == 0)
                    bookmark->filename = newFilename;
            }
        }
    }
    // What was remembered of the merges of the file is about the same bookmarks under
    // the old name: move the records to the new name, or undoing a merge after the
    // rename can't find them (the split is then reported under the new name).
    auto mergedIt = mMergedBookmarks.find(oldFilename);
    if (mergedIt != mMergedBookmarks.end()) {
        QList<MergedLines> merges = mergedIt.value();
        mMergedBookmarks.erase(mergedIt);
        mMergedBookmarks[newFilename].append(merges);
    }
}

void BookmarkModel::clear(bool forProject)
{
    if (forProject==mIsForProject)
        beginResetModel();
    if (forProject)
        mProjectBookmarks.clear();
    else
        mBookmarks.clear();
    // The bookmarks this model had are gone: the records of their merges would only
    // hold them alive and could be matched by a split of another version of the file
    // (see onFileSplitLines()). The other model still has its own bookmarks, so only
    // its records must be kept.
    auto mergedIt = mMergedBookmarks.begin();
    while (mergedIt != mMergedBookmarks.end()) {
        QList<MergedLines>& merges = mergedIt.value();
        for (int i=merges.count()-1;i>=0;i--) {
            if (merges[i].forProject == forProject)
                merges.removeAt(i);
        }
        if (merges.isEmpty())
            mergedIt = mMergedBookmarks.erase(mergedIt);
        else
            ++mergedIt;
    }
    for (int i=mBookmarksPutBackBySplit.count()-1;i>=0;i--) {
        if (mBookmarksPutBackBySplit[i].forProject == forProject)
            mBookmarksPutBackBySplit.removeAt(i);
    }
    if (forProject==mIsForProject)
        endResetModel();
}

bool BookmarkModel::updateDescriptionAt(int row, const QString &description, bool forProject)
{
    QList<PBookmark>& bookmarks = forProject ? mProjectBookmarks : mBookmarks;
    if (row<0 || row>=bookmarks.count())
        return false;
    bookmarks[row]->description = description;
    if (forProject == mIsForProject)
        emit dataChanged(index(row, 0), index(row, columnCount(QModelIndex())-1));
    return true;
}

bool BookmarkModel::updateDescription(const QString &filename, int line, const QString &description, bool forProject)
{
    QList<PBookmark> bookmarks;
    if (forProject)
        bookmarks = mProjectBookmarks;
    else
        bookmarks = mBookmarks;
    for (int i=0;i<bookmarks.count();i++) {
        PBookmark bookmark = bookmarks[i];
        if (bookmark->filename.compare(filename, PATH_SENSITIVITY) == 0
                && bookmark->line == line) {
            bookmark->description = description;
            emit dataChanged(createIndex(i,0),createIndex(i,2));
            return true;
        }
    }
    return false;
}

bool BookmarkModel::updateDescription(const QString &filename, int line, const QString &description)
{
    return updateDescription(filename,line,description,mIsForProject);
}

void BookmarkModel::saveBookmarks(const QString &filename)
{
    save(filename,QString());
}

void BookmarkModel::loadBookmarks(const QString &filename)
{
    if (!mIsForProject)
        beginResetModel();
    qint64 t;
    mLastLoadBookmarksTimestamp = QDateTime::currentMSecsSinceEpoch();
    mBookmarks = load(filename,0,&t);
    if (!mIsForProject)
        endResetModel();
}

void BookmarkModel::save(const QString &filename, const QString& projectFolder)
{
    bool forProject = !projectFolder.isEmpty();
    qint64 t,fileTimestamp;
    QHash<QString,int> compareHash;
    QList<PBookmark> &list=forProject?mProjectBookmarks:mBookmarks;
    if (forProject) {
        t=mLastLoadProjectBookmarksTimestamp;
    } else {
        t=mLastLoadBookmarksTimestamp;
    }
    for (int i=0;i<list.count();i++) {
        const PBookmark& bookmark=list[i];
        QString filename = forProject?extractRelativePath(projectFolder, bookmark->filename):bookmark->filename;
        QString key = QString("%1-%2").arg(filename).arg(bookmark->line);
        compareHash.insert(key,i);
    }
    QList<PBookmark> fileBookmarks;
    try {
        fileBookmarks=load(filename, t,&fileTimestamp);
    } catch (FileError& e) {

    }

    QFile file(filename);
    if (file.open(QFile::WriteOnly | QFile::Truncate)) {        

//        QList<PBookmark> saveBookmarks;

        QDir dir(projectFolder);
        foreach (const PBookmark& bookmark, fileBookmarks) {
            QString key = QString("%1-%2").arg(bookmark->filename).arg(bookmark->line);
            int idx = compareHash.value(key,-1);
            if (idx<0) {
                int count=list.count();
                compareHash.insert(key,count);
                if (forProject == mIsForProject) {
                    beginInsertRows(QModelIndex(),count,count);
                }
                if (forProject) {
                    bookmark->filename = cleanPath(dir.absoluteFilePath(bookmark->filename));
                }

                list.append(bookmark);
                if (forProject == mIsForProject)
                    endInsertRows();
            } else {
                PBookmark pTemp = list[idx];
                if (pTemp->timestamp<=bookmark->timestamp) {
                    bookmark->description = pTemp->description;
                    bookmark->timestamp = pTemp->timestamp;
                    if (forProject == mIsForProject)
                        emit dataChanged(createIndex(idx,2),createIndex(idx,2));
                }
            }
        }

        qint64 saveTime = QDateTime::currentMSecsSinceEpoch();;
        if (forProject) {
            mLastLoadProjectBookmarksTimestamp = saveTime;
        } else {
            mLastLoadBookmarksTimestamp = saveTime;
        }

        QJsonObject rootObj;
        rootObj["timestamp"]=QString("%1").arg(saveTime);
        QJsonArray array;
        foreach (const PBookmark& bookmark, list) {
            QJsonObject obj;
            QString filename = forProject?extractRelativePath(projectFolder, bookmark->filename):bookmark->filename;
            obj["filename"]=filename;
            obj["line"]=bookmark->line;
            obj["description"]=bookmark->description;
            obj["timestamp"]=QString("%1").arg(bookmark->timestamp);
            array.append(obj);
        }
        rootObj["bookmarks"]=array;
        QJsonDocument doc;
        doc.setObject(rootObj);
        if (file.write(doc.toJson())<0) {
            throw FileError(tr("Save file '%1' failed.")
                            .arg(filename));
        }
    } else {
        throw FileError(tr("Can't open file '%1' for write.")
                        .arg(filename));
    }
}

QList<PBookmark> BookmarkModel::load(const QString& filename, qint64 criteriaTimestamp, qint64* pFileTimestamp)
{
    //clear(forProject);
    QList<PBookmark> bookmarks;
    QFile file(filename);
    if (!file.exists())
        return bookmarks;
    if (file.open(QFile::ReadOnly)) {
        QByteArray content = file.readAll().trimmed();
        if (content.isEmpty())
            return bookmarks;
        QJsonParseError error;
        QJsonDocument doc(QJsonDocument::fromJson(content,&error));
        if (error.error  != QJsonParseError::NoError) {
            throw FileError(tr("Error in json file '%1':%2 : %3")
                            .arg(filename)
                            .arg(error.offset)
                            .arg(error.errorString()));
        }
        QJsonObject rootObj=doc.object();
        bool ok;
        qint64 timestamp = rootObj["timestamp"].toString().toLongLong(&ok);
        if (!ok || timestamp<=criteriaTimestamp)
            return bookmarks;
        *pFileTimestamp=timestamp;
        QJsonArray array = rootObj["bookmarks"].toArray();
        for  (int i=0;i<array.count();i++) {
            QJsonValue value = array[i];
            QJsonObject obj=value.toObject();
            qint64 bookmarkTimestamp = obj["timestamp"].toString().toULongLong(&ok);
            if (ok && bookmarkTimestamp>criteriaTimestamp) {
                PBookmark bookmark = std::make_shared<Bookmark>();
                bookmark->filename = obj["filename"].toString();
                bookmark->line = obj["line"].toInt();
                bookmark->description = obj["description"].toString();
                bookmark->timestamp=obj["timestamp"].toString().toULongLong();
                bookmarks.append(bookmark);
            }
        }
    } else {
        throw FileError(tr("Can't open file '%1' for read.")
                        .arg(filename));
    }
    return bookmarks;
}

void BookmarkModel::saveProjectBookmarks(const QString &filename, const QString& projectFolder)
{
    save(filename,projectFolder);
}

void BookmarkModel::loadProjectBookmarks(const QString &filename, const QString& projectFolder)
{
    if (mIsForProject)
        beginResetModel();
    qint64 t;
    mLastLoadProjectBookmarksTimestamp = QDateTime::currentMSecsSinceEpoch();
    mProjectBookmarks = load(filename,0,&t);
    QDir folder(projectFolder);
    foreach (PBookmark bookmark, mProjectBookmarks) {
        bookmark->filename=cleanPath(folder.absoluteFilePath(bookmark->filename));
    }
    if (mIsForProject)
        endResetModel();
}

void BookmarkModel::onFileDeleteLines(const QString &filename, int startLine, int count, bool forProject)
{
    QList<PBookmark> bookmarks;
    if (forProject)
        bookmarks = mProjectBookmarks;
    else
        bookmarks = mBookmarks;
    for (int i = bookmarks.count()-1;i>=0;i--){
        PBookmark bookmark = bookmarks[i];
        if  (bookmark->filename == filename
             && bookmark->line>=startLine) {
            if (bookmark->line >= startLine+count) {
                bookmark->line -= count;
                if (forProject == mIsForProject)
                    emit dataChanged(createIndex(i,0),createIndex(i,2));
            } else {
                removeBookmarkAt(i,forProject);
            }
        }
    }
}

void BookmarkModel::onFileInsertLines(const QString &filename, int startLine, int count, bool forProject)
{
    QList<PBookmark> bookmarks;
    if (forProject)
        bookmarks = mProjectBookmarks;
    else
        bookmarks = mBookmarks;
    for (int i = 0; i<bookmarks.count();i++){
        PBookmark bookmark = bookmarks[i];
        if  (bookmark->filename == filename
             && bookmark->line>=startLine) {
            bookmark->line+=count;
            if (forProject == mIsForProject)
                emit dataChanged(createIndex(i,0),createIndex(i,2));
        }
    }
}

void BookmarkModel::onFileLineMoved(const QString &filename, int fromLine, int toLine, bool forProject)
{
    QList<PBookmark> bookmarks;
    if (forProject)
        bookmarks = mProjectBookmarks;
    else
        bookmarks = mBookmarks;
    for (int i = 0; i<bookmarks.count();i++){
        bool changed = false;
        PBookmark bookmark = bookmarks[i];
        if  (bookmark->filename == filename) {
           if (bookmark->line==fromLine) {
               bookmark->line = toLine;
               changed = true;
           } else if (fromLine < toLine) {
               if (fromLine < bookmark->line && bookmark->line <= toLine) {
                   --bookmark->line;
                   changed = true;
               }
           } else if (toLine < fromLine) {
               if (toLine <= bookmark->line && bookmark->line <= fromLine) {
                   ++bookmark->line;
                   changed = true;
               }
           }
        }
        if (changed && forProject == mIsForProject)
            emit dataChanged(createIndex(i,0),createIndex(i,2));
    }
}

void BookmarkModel::onFileMergeLines(const QString &filename, int removedLine, int intoLine, int lineCount, bool forProject)
{
    // Same as BreakpointModel::onFileMergeLines(): the bookmark follows the text of the
    // line it is set on, which is merged into the line above instead of being lost.
    // Several bookmarks are allowed on one line now, so all the bookmarks of the
    // removed line follow the merged code onto that same line; they do not have to be
    // pushed to separate lines any more.
    Q_UNUSED(lineCount);
    QList<PBookmark> bookmarks;
    if (forProject)
        bookmarks = mProjectBookmarks;
    else
        bookmarks = mBookmarks;
    QList<PBookmark> moved;
    for (int i = 0; i<bookmarks.count();i++){
        PBookmark bookmark = bookmarks[i];
        if  (bookmark->filename == filename
             && bookmark->line == removedLine) {
            bookmark->line = intoLine;
            moved.append(bookmark);
            if (forProject == mIsForProject)
                emit dataChanged(createIndex(i,0),createIndex(i,2));
        }
    }
    // Remember them and the line they were on: undoing that merge puts them back on
    // their own line (see onFileSplitLines()), and nothing else in the model knows
    // where they were. Merges are undone in the reverse order they were done in, so
    // they are kept as a stack, innermost last.
    if (!moved.isEmpty())
        mMergedBookmarks[filename].append(MergedLines{removedLine, moved, forProject});
}

void BookmarkModel::onFileSplitLines(const QString &filename, int mergedLine, int newLine, bool forProject)
{
    QMap<QString, QList<MergedLines>>::iterator it = mMergedBookmarks.find(filename);
    if (it==mMergedBookmarks.end())
        return;
    QList<MergedLines> &merges = it.value();
    // The split being reported undoes the merge that removed `newLine`; that merge is
    // the innermost (last) one that did. Any other one is none of our business.
    int mergeIndex = -1;
    for (int i = merges.count()-1;i>=0;i--) {
        if (merges[i].forProject == forProject && merges[i].removedLine == newLine) {
            mergeIndex = i;
            break;
        }
    }
    if (mergeIndex<0)
        return;
    QList<PBookmark> moved = merges.takeAt(mergeIndex).bookmarks;
    QList<PBookmark> bookmarks;
    if (forProject)
        bookmarks = mProjectBookmarks;
    else
        bookmarks = mBookmarks;
    for (int i = 0; i<bookmarks.count();i++){
        PBookmark bookmark = bookmarks[i];
        // The bookmark must still be a bookmark of that file (if it is not, it was
        // removed in between, or removed and set again: the merge is not about it any
        // more) and still be on the merged line or below it. Being the exact bookmark
        // the merge moved, it is put back on the line it was set on, whatever the edits
        // in between did to its line.
        if (!moved.contains(bookmark) || bookmark->filename != filename
                || bookmark->line < mergedLine)
            continue;
        bookmark->line = newLine;
        // Its line is exact now: the content change this split runs in ends with
        // moveBookmarksInFile(), which must not look it up in the line map again.
        mBookmarksPutBackBySplit.append(PutBackBookmark{bookmark, forProject});
        if (forProject == mIsForProject)
            emit dataChanged(createIndex(i,0),createIndex(i,2));
    }
}

void BookmarkModel::removeBookmarkAt(int i, bool forProject)
{
    // What was remembered of the merges this bookmark went through (and of the split
    // that put it back on its own line) is about the bookmark dropped here: keeping it
    // would hold the bookmark alive, and the record of a merge that has no bookmark left
    // to put back would be the one the next split of that line finds (see
    // onFileSplitLines()). The whole map is looked at: what a merge was remembered under
    // is the name the file had then, which a rename may have changed since.
    PBookmark removed;
    if (forProject)
        removed = mProjectBookmarks[i];
    else
        removed = mBookmarks[i];
    for (int j=mBookmarksPutBackBySplit.count()-1;j>=0;j--) {
        if (mBookmarksPutBackBySplit[j].bookmark == removed)
            mBookmarksPutBackBySplit.removeAt(j);
    }
    const QStringList names = mMergedBookmarks.keys();
    for (const QString& name: names) {
        auto it = mMergedBookmarks.find(name);
        if (it == mMergedBookmarks.end())
            continue;
        QList<MergedLines>& merges = it.value();
        for (int j=merges.count()-1;j>=0;j--) {
            merges[j].bookmarks.removeAll(removed);
            if (merges[j].bookmarks.isEmpty())
                merges.removeAt(j);
        }
        if (merges.isEmpty())
            mMergedBookmarks.erase(it);
    }
    if (forProject == mIsForProject)
        beginRemoveRows(QModelIndex(), i,i);
    if (forProject)
        mProjectBookmarks.removeAt(i);
    else
        mBookmarks.removeAt(i);
    if (forProject == mIsForProject)
        endRemoveRows();
}

void BookmarkModel::removeBookmarkAt(int i)
{
    return removeBookmarkAt(i,isForProject());
}

bool BookmarkModel::isBookmarkExists(const QString &filename, int line, bool forProject)
{
    QList<PBookmark> bookmarks;
    if (forProject)
        bookmarks = mProjectBookmarks;
    else
        bookmarks = mBookmarks;
    foreach (const PBookmark& bookmark, bookmarks) {
        if (bookmark->filename.compare(filename, PATH_SENSITIVITY) == 0
                && bookmark->line == line) {
            return true;
        }
    }
    return false;
}

bool BookmarkModel::isForProject() const
{
    return mIsForProject;
}

void BookmarkModel::setIsForProject(bool newIsForProject)
{
    if (newIsForProject!=mIsForProject) {
        mIsForProject = newIsForProject;
        beginResetModel();
        endResetModel();
    }
}

void BookmarkModel::sort(int column, Qt::SortOrder order)
{
    switch(column) {
    case 0:
        if (order == Qt::SortOrder::AscendingOrder) {
            auto sorter=[](PBookmark b1,PBookmark b2) {
                return b1->description < b2->description;
            };
            std::sort(mBookmarks.begin(),mBookmarks.end(),sorter);
            std::sort(mProjectBookmarks.begin(),mProjectBookmarks.end(),sorter);
        } else {
            auto sorter=[](PBookmark b1,PBookmark b2) {
                return b2->description<b1->description;
            };
            std::sort(mBookmarks.begin(),mBookmarks.end(),sorter);
            std::sort(mProjectBookmarks.begin(),mProjectBookmarks.end(),sorter);
        }
        break;
    case 1:
        if (order == Qt::SortOrder::AscendingOrder) {
            auto sorter=[](PBookmark b1,PBookmark b2) {
                return b1->filename<b2->filename;
            };
            std::sort(mBookmarks.begin(),mBookmarks.end(),sorter);
            std::sort(mProjectBookmarks.begin(),mProjectBookmarks.end(),sorter);
        } else {
            auto sorter=[](PBookmark b1,PBookmark b2) {
                return b2->filename<b1->filename;
            };
            std::sort(mBookmarks.begin(),mBookmarks.end(),sorter);
            std::sort(mProjectBookmarks.begin(),mProjectBookmarks.end(),sorter);
        }
        break;
    case 2:
        if (order == Qt::SortOrder::AscendingOrder) {
            auto sorter=[](PBookmark b1,PBookmark b2) {
                return b1->line<b2->line;
            };
            std::sort(mBookmarks.begin(),mBookmarks.end(),sorter);
            std::sort(mProjectBookmarks.begin(),mProjectBookmarks.end(),sorter);
        } else {
            auto sorter=[](PBookmark b1,PBookmark b2) {
                return b2->line<b1->line;
            };
            std::sort(mBookmarks.begin(),mBookmarks.end(),sorter);
            std::sort(mProjectBookmarks.begin(),mProjectBookmarks.end(),sorter);
        }
        break;
    }
}

int BookmarkModel::rowCount(const QModelIndex &) const
{
    if (mIsForProject)
        return mProjectBookmarks.count();
    else
        return mBookmarks.count();
}

QVariant BookmarkModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();
    int row = index.row();
    PBookmark bookmark;
    if (mIsForProject)
        bookmark = mProjectBookmarks[row];
    else
        bookmark = mBookmarks[row];
    if (role == Qt::DisplayRole) {
        switch(index.column()) {
        case 0:
            return bookmark->description;
        case 1:
            return bookmark->line+1;
        case 2:
            return bookmark->filename;
        }
    }
    return QVariant();
}

int BookmarkModel::columnCount(const QModelIndex &) const
{
    return 3;
}

QVariant BookmarkModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal) {
        if (role == Qt::DisplayRole) {
            switch(section) {
            case 0:
                return tr("Description");
            case 1:
                return tr("Line");
            case 2:
                return tr("Filename");
            }
        }
    }
    return QVariant();
}

