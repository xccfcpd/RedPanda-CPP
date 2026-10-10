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
#include "editoranchors.h"

// the keys of the anchors are the fingerprints of the models (see
// EditorAnchors::lineKey()), and a line joined by an edit is followed through
// findLineHoldingMostOfTheCode()
#include "debugger/breakpointanchor.h"
#include "debugger/debuggermodels.h"

#include <QDebug>

QString EditorAnchors::lineKey(const QString& lineText)
{
    return breakpointFingerprint(lineText);
}

ReformatAnchor EditorAnchors::lineAnchor(const QString& previousLineText,
                                         const QString& lineText,
                                         const QString& nextLineText)
{
    ReformatAnchor anchor;
    anchor.line = lineKey(lineText);
    // previous, this and next line: the context tells repeated lines (many
    // "return 0;") and very short lines (a lone "}") apart.
    // Built with the model's own helper instead of joining the three keys here:
    // an editor line and a breakpoint line must be matched the same way (see
    // test_lineAnchor_matchesTheBreakpointAnchor()), and going through the single
    // implementation makes them agree by construction, not just by that test.
    anchor.context = breakpointContextFingerprint(previousLineText, lineText, nextLineText);
    return anchor;
}

QMap<int,int> EditorAnchors::remapLines(const QMap<int,ReformatAnchor> &anchors,
                                        const QStringList &content)
{
    QMap<int,int> result;
    if (anchors.isEmpty())
        return result;

    int lineCount = content.count();
    // tier 1: previous + this + next line; tier 2: the line's own text
    QMap<QString,QList<int>> contextIndex;
    QMap<QString,QList<int>> lineIndex;
    for (int line=0;line<lineCount;line++) {
        ReformatAnchor anchor = lineAnchor(line>0? content[line-1] : QString(),
                                           content[line],
                                           line+1<lineCount? content[line+1] : QString());
        // an empty line has no usable anchor, and its context would match every
        // other empty line
        if (anchor.line.isEmpty())
            continue;
        contextIndex[anchor.context].append(line);
        // a very short line (e.g. "}") matches too many lines on its own; it is
        // handled through the context instead
        if (anchor.line.length()>1)
            lineIndex[anchor.line].append(line);
    }

    auto findNearest = [lineCount](const QMap<QString,QList<int>> &index,
                                   const QString &key, int oldLine) {
        if (key.isEmpty())
            return -1;
        auto candidates = index.constFind(key);
        if (candidates==index.constEnd())
            return -1;
        int newLine = -1;
        int minDistance = lineCount+1;
        foreach(int candidate, *candidates) {
            int distance = qAbs(candidate-oldLine);
            if (distance<minDistance) {
                minDistance = distance;
                newLine = candidate;
            }
        }
        return newLine;
    };

    QList<int> unmappedLines;
    for (auto it=anchors.constBegin();it!=anchors.constEnd();++it) {
        int oldLine = it.key();
        int newLine = -1;
        if (!it.value().line.isEmpty()) {
            newLine = findNearest(contextIndex, it.value().context, oldLine);
            if (newLine<0 && it.value().line.length()>1)
                newLine = findNearest(lineIndex, it.value().line, oldLine);
            if (newLine<0 && it.value().line.length()>1) {
                // tier 3: before the content was replaced an edit joined the line with
                // its neighbors (the models follow the merge and move their markers to
                // the line it happened on - see BreakpointModel::onFileMergeLines()),
                // so the anchor's text is not the text of any line any more. The code
                // is still there, at the head of one of the new lines (the replacement
                // splits the joined line back): follow it there, instead of leaving
                // the marker on the line number the merge put it on.
                newLine = findLineHoldingMostOfTheCode(content, it.value().line);
            }
        }
        if (newLine<0) {
            newLine = qBound(0, oldLine, lineCount>0? lineCount-1 : 0);
            unmappedLines.append(oldLine);
        }
        result.insert(oldLine, newLine);
    }
    if (!unmappedLines.isEmpty()) {
        qDebug() << "reformat: no text anchor found for" << unmappedLines.count()
                 << "line(s), keeping the old line number(s):" << unmappedLines;
    }
    return result;
}
