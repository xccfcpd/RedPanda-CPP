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
#ifndef BOOKMARKMODEL_H
#define BOOKMARKMODEL_H

#include <QAbstractTableModel>
#include <memory>
#include <QDebug>
#include <QMap>

struct Bookmark {
    QString filename;
    int line;
    QString description;
    qint64 timestamp;
};

using PBookmark=std::shared_ptr<Bookmark>;

class BookmarkModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    BookmarkModel(QObject* parent=nullptr);
    QSet<int> bookmarksInFile(const QString& filename, bool forProject);
    void addBookmark(const QString&filename, int line, const QString& description, bool forProject);
    PBookmark bookmark(int i, bool forProject);
    PBookmark bookmark(int i);
    PBookmark bookmark(const QString&filename, int line, bool forProject);
    PBookmark bookmark(const QString&filename, int line);
    bool removeBookmark(const QString&filename, int line, bool forProject);
    void removeBookmarks(const QString& filename, bool forProject);
    // Moves the bookmarks of a file to the lines given by "lineMap" (old line ->
    // new line). Used when the whole content of the file has been replaced (see
    // Editor::isReplacingContent()): the bookmarks follow the code they are on
    // instead of being dropped by the line bookkeeping. Lines that are not in the
    // map keep their line number. "lineCount" is the number of lines of the new
    // content: a bookmark that would collide with another one is pushed to the
    // next free line, but never past that last line. The bookmarks that
    // onFileSplitLines() put back on their own line are left on it (see
    // mBookmarksPutBackBySplit).
    void moveBookmarksInFile(const QString& filename, const QMap<int,int>& lineMap,
                             int lineCount, bool forProject);
    void renameBookmarkFile(const QString& oldFilename, const QString& newFilename, bool forProject);
    void clear(bool forProject);
    bool updateDescription(const QString&filename, int line, const QString& description, bool forProject);
    bool updateDescription(const QString&filename, int line, const QString& description);
    void saveBookmarks(const QString& filename);
    void loadBookmarks(const QString& filename);
    void saveProjectBookmarks(const QString& filename, const QString& projectFolder);
    void loadProjectBookmarks(const QString& filename, const QString& projectFolder);
    void removeBookmarkAt(int i, bool forProject);
    void removeBookmarkAt(int i);
public slots:
    void onFileDeleteLines(const QString& filename, int startLine, int count, bool forProject);
    void onFileInsertLines(const QString& filename, int startLine, int count, bool forProject);
    void onFileLineMoved(const QString& filename, int fromLine, int toLine, bool forProject);
    void onFileMergeLines(const QString& filename, int removedLine, int intoLine, bool forProject);
    // Puts the bookmarks that onFileMergeLines() moved onto the merged line back on
    // their own line, when that merge is undone (the editor reports the line split
    // back into it and `newLine`). A bookmark has no fingerprint to find its code
    // with, like a breakpoint does: what was remembered of the merge is the only
    // memory of where it was.
    void onFileSplitLines(const QString& filename, int mergedLine, int newLine, bool forProject);
private:
    bool isBookmarkExists(const QString&filename, int line, bool forProject);
    void save(const QString& filename, const QString& projectFolder);
    QList<PBookmark> load(const QString& filename, qint64 criteriaTimestamp, qint64* pFileTimestamp);

private:
    QList<PBookmark> mBookmarks;
    QList<PBookmark> mProjectBookmarks;
    // What the merges moved, per file (innermost last): the bookmarks that were on the
    // line that disappeared and the line they came from, so that undoing a merge puts
    // them back where they were (see onFileSplitLines()). A bookmark has no fingerprint
    // to find its code with, like a breakpoint does.
    struct MergedLines {
        int removedLine = 0;
        QList<PBookmark> bookmarks;
    };
    QMap<QString, QList<MergedLines>> mMergedBookmarks;
    // The bookmarks that the undoing of a merge put back on their own line (see
    // onFileSplitLines()): the split happens inside the content change that undoes the
    // merge, and that change ends with moveBookmarksInFile(). Their line is exact then,
    // while the line map is keyed by the lines the old content had: the line one of
    // them sits on may be the line another marker was only passing through during the
    // merge, and moving it by the map would send it to that one's code. Removed by the
    // moveBookmarksInFile() call of the file they belong to.
    QList<PBookmark> mBookmarksPutBackBySplit;
    qint64 mLastLoadBookmarksTimestamp;
    qint64 mLastLoadProjectBookmarksTimestamp;
    bool mIsForProject;

    // QAbstractItemModel interface
public:
    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    int columnCount(const QModelIndex &parent) const override;

    // QAbstractItemModel interface
public:
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    bool isForProject() const;
    void setIsForProject(bool newIsForProject);

    // QAbstractItemModel interface
public:
    void sort(int column, Qt::SortOrder order) override;
};

#endif // BOOKMARKMODEL_H
