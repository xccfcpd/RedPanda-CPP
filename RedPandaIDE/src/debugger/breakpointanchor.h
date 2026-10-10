#ifndef BREAKPOINTANCHOR_H
#define BREAKPOINTANCHOR_H

#include "debuggermodels.h"

#include <QString>
#include <QStringList>

// Finding back, inside a new content, the line a breakpoint was set on. The line
// is remembered by the normalized text it holds and by the text of its neighbours
// (see breakpointFingerprint() and breakpointContextFingerprint() in
// debuggermodels.h), so that repeated lines and very short lines can be told
// apart. Header-only, so that it is tested without the debugger sources - like
// EditorAnchors, on the editor side.

// Fingerprint of a line of the given content (empty if the line doesn't exist).
inline QString contentFingerprint(const QStringList& content, int line)
{
    if (line<0 || line>=content.count())
        return QString();
    return breakpointFingerprint(content[line]);
}

// Anchor of a line of the given content, as stored in Breakpoint::fingerprint.
inline QString contentContextFingerprint(const QStringList& content, int line)
{
    if (line<0 || line>=content.count())
        return QString();
    return breakpointContextFingerprint(line>0? content[line-1] : QString(),
                                        content[line],
                                        line+1<content.count()? content[line+1] : QString());
}

// The line fingerprint inside a stored anchor: the middle field of a context
// fingerprint, or the whole value when it is a plain line fingerprint (a
// breakpoint stored by an older version).
inline QString lineFingerprintOf(const QString& fingerprint)
{
    QStringList parts = fingerprint.split(QLatin1Char('\n'));
    if (parts.count()==3)
        return parts[1];
    return fingerprint;
}

// Whether the line of the content really is the line of the code the anchor is about,
// and may therefore replace that anchor. The anchor is the memory of the code: it is
// what finds the code back once the content is not the one it was taken on any more.
// An edit joining the line with its neighbours leaves the code inside a line holding
// the text of a neighbour too (see BreakpointModel::onFileMergeLines()), and an edit
// splitting the line leaves only a piece of the code on it: storing the anchor of
// either would store text no line will ever hold again, the memory of the code would
// be lost, and the breakpoint would then drift from line to line as the following
// edits come in (Ctrl+Z after Ctrl+Z). A reformat rewriting the line (a brace in front
// of the code, a semicolon moved around) is neither of those: that line still is the
// line of the code, and its new text is worth remembering - it is what makes a
// forgotten anchor good again.
inline bool lineIsTheLineOfTheCode(const QString& lineFingerprint,
                                   const QString& fingerprint)
{
    QStringList parts = fingerprint.split(QLatin1Char('\n'));
    if (parts.count()!=3 || parts[1].isEmpty())
        return true;
    const QString& code = parts[1];
    if (lineFingerprint==code)
        return true;
    if (code.contains(lineFingerprint))
        // only a piece of the code is on that line (the line was split)
        return false;
    return !((!parts[0].isEmpty() && lineFingerprint.startsWith(parts[0]+code))
          || (!parts[2].isEmpty() && lineFingerprint.endsWith(code+parts[2])));
}

// Nearest line of the content whose anchor is "fingerprint", or -1 if there is
// none. "useContext" chooses between the context anchors (the line together with
// its neighbours) and the plain line anchors. A plain anchor of one character (a
// lone "}") matches too many places to be relied on, so it is never searched for.
// With "uniqueOnly", an anchor that matches more than one line is ambiguous and
// isn't used at all: that keeps a plain line anchor from moving a breakpoint to
// another line that happens to hold the same code.
inline int findNearestLineByAnchor(const QStringList& content, const QString& fingerprint,
                                   int nearLine, bool useContext, bool uniqueOnly = false)
{
    if (fingerprint.isEmpty())
        return -1;
    if (!useContext && fingerprint.length()<=1)
        return -1;
    int result = -1;
    int matches = 0;
    int minDistance = content.count()+1;
    for (int line=0;line<content.count();line++) {
        QString anchor = useContext? contentContextFingerprint(content, line)
                                   : contentFingerprint(content, line);
        if (anchor!=fingerprint)
            continue;
        matches++;
        int distance = qAbs(line-nearLine);
        if (distance<minDistance) {
            minDistance = distance;
            result = line;
        }
    }
    if (uniqueOnly && matches>1)
        return -1;
    return result;
}

// Length of the longest beginning of "code" that occurs somewhere in "line": how
// much of the code the line holds, from the first character of the code on. Used
// when no line holds the code on its own any more.
inline int sharedPrefixLength(const QString& line, const QString& code)
{
    int best = 0;
    for (int start=0;start<line.length();start++) {
        if (line.at(start)!=code.at(0))
            continue;
        int length = 0;
        while (length<code.length() && start+length<line.length()
               && line.at(start+length)==code.at(length))
            length++;
        if (length>best)
            best = length;
    }
    return best;
}

// The line of the content holding most of the code, or -1 when no line does or when
// the best one is not clearly ahead of the others. A line holding a couple of
// characters of the code ("cout <<", a lone "}") recognizes nothing and is refused;
// so is a line the code is shared with, evenly: leaving the breakpoint on the line
// number it had is then safer than guessing between two lines.
inline int findLineHoldingMostOfTheCode(const QStringList& content, const QString& code)
{
    // Together with the "uniqueOnly" of the anchor searches, this keeps a few
    // characters shared by common code (a call, a keyword) from moving anything.
    const int minShared = 8;
    if (code.length()<minShared)
        return -1;
    int best = -1;
    int bestLength = 0;
    int secondLength = 0;
    for (int line=0;line<content.count();line++) {
        int shared = sharedPrefixLength(contentFingerprint(content, line), code);
        if (shared>bestLength) {
            secondLength = bestLength;
            bestLength = shared;
            best = line;
        } else if (shared>secondLength) {
            secondLength = shared;
        }
    }
    // half of the code at least: a line that shares less than that holds something
    // else that starts like the code
    if (bestLength<minShared || bestLength*2<code.length() || bestLength<=secondLength)
        return -1;
    return best;
}

// What re-anchoring one breakpoint gives: the line it is on now, the anchor to
// store from now on, and whether any of the two changed.
struct ReanchoredBreakpoint {
    int line;
    QString fingerprint;
    bool changed;
};

// Moves the breakpoint (line, fingerprint) on the code it was set on, inside the
// new content. A stored anchor that can't be found any more leaves the breakpoint
// where it is *and keeps the anchor*: the code can come back (a line merge undone,
// an edit undone), and an anchor is the only memory of it.
inline ReanchoredBreakpoint reanchorBreakpoint(const QStringList& content, int line,
                                               const QString& fingerprint)
{
    ReanchoredBreakpoint result{line, fingerprint, false};
    QString current = contentContextFingerprint(content, line);
    if (fingerprint.isEmpty()) {
        // no fingerprint to look for (the breakpoint comes from an older config
        // file): remember the code it is on, so it can follow it from now on
        result.fingerprint = current;
        result.changed = true;
    } else if (fingerprint!=current) {
        // The line doesn't hold the code the breakpoint was set on any more. Look
        // for it: first with the neighbours (they tell repeated code lines apart),
        // then with the line text alone (which also handles fingerprints stored
        // before the neighbours were kept).
        int newLine = findNearestLineByAnchor(content, fingerprint, line, true);
        if (newLine<0)
            newLine = findNearestLineByAnchor(content, lineFingerprintOf(fingerprint), line,
                                              false, true);
        if (newLine<0) {
            // The code is nowhere in this content as a line: it is not in the file
            // any more, or it is *inside* a line that holds more than it did. The
            // second case is the usual one - a line was merged with another one, and
            // a reformat then split the merged line back (see
            // Editor::finishReformat()): the code is on a line of its own again, but
            // that line holds more than the code (the reformat indents it, puts the
            // code of the line it was merged with in front of it, rewrites its end),
            // so no anchor matches it. The line holding most of the code is then the
            // only thing left to recognize it by.
            newLine = findLineHoldingMostOfTheCode(content, lineFingerprintOf(fingerprint));
            if (newLine==line) {
                // The code is inside the line the breakpoint is already on (that line
                // was merged with the one holding the code): it is where it belongs,
                // and the anchor is kept - it is the memory of the code that puts the
                // breakpoint back on its own line when the merge is undone.
                return result;
            }
        }
        if (newLine<0) {
            // The code is not there at all. The breakpoint is never dropped, and it
            // keeps its anchor instead of remembering the line it happens to be on:
            // that anchor still tells the code it was set on, so the breakpoint is
            // put back on it as soon as the content holds it again (undoing the
            // merge, for one).
            return result;
        }
        if (newLine!=line)
            result.line = newLine;
        if (lineIsTheLineOfTheCode(contentFingerprint(content, result.line), fingerprint)) {
            // the line is the line of the code: remember it and its neighbours from now
            // on, whatever the edit did to the rest of its text (a reformat, typically)
            result.fingerprint = contentContextFingerprint(content, result.line);
            result.changed = true;
        } else {
            // The code is only *inside* that line (an edit joined the lines, or split
            // the one the code was on). Keep the anchor: it is the memory of the code,
            // and it puts the breakpoint back on its own line as soon as the content
            // holds the code on its own again (undoing the edit, for one).
            result.changed = newLine!=line;
        }
    }
    return result;
}

#endif // BREAKPOINTANCHOR_H
