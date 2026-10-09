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
#ifndef EDITORANCHORS_H
#define EDITORANCHORS_H

#include <QString>
#include <QStringList>
#include <QMap>

// How a line is described so that it can be found back after the whole content of
// a document has been replaced (reloaded from the disk, reformatted, replaced by
// the output of a tool, undone/redone). The breakpoints, the bookmarks and the
// carets of a file are all anchored that way and put back on the code they were
// on once the new content is in place.
// The algorithms live outside of Editor, which needs a QSynedit document, so that
// they can be tested on plain text.
struct ReformatAnchor {
    // the text of the line itself, without the whitespace
    QString line;
    // the text of the previous, this and next line, so that repeated lines (many
    // "return 0;") and very short ones (a lone "}") can be told apart
    QString context;
};

namespace EditorAnchors {

// The text of a line without the whitespace, capped like the fingerprint of a
// breakpoint (see breakpointFingerprint()): a very long line is then described by
// the same key in the editor and in the models.
QString lineKey(const QString& lineText);

// The anchor of a line, given the text of its neighbours. An empty string stands
// for a line that is empty, or for one that is outside of the document. Built on
// breakpointFingerprint()/breakpointContextFingerprint(), so that the editor and
// the breakpoint model describe a line by the same string.
ReformatAnchor lineAnchor(const QString& previousLineText,
                          const QString& lineText,
                          const QString& nextLineText);

// Maps every line of "anchors" to the line of "content" that holds the same code,
// choosing the candidate closest to the original line. The neighbours are tried
// first (they pin repeated lines down), then the line itself. A line without a
// usable anchor (an empty line, or one that can't be found back) keeps its original
// number and is reported in the debug output.
QMap<int,int> remapLines(const QMap<int,ReformatAnchor>& anchors,
                         const QStringList& content);

}

#endif // EDITORANCHORS_H
