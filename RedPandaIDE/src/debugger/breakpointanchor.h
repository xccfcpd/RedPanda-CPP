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

// What re-anchoring one breakpoint gives: the line it is on now, the anchor to
// store from now on, and whether any of the two changed.
struct ReanchoredBreakpoint {
    int line;
    QString fingerprint;
    bool changed;
};

// Moves the breakpoint (line, fingerprint) on the code it was set on, inside the
// new content. A stored anchor that can't be found any more leaves the breakpoint
// where it is: it is never dropped, it just follows the code that is there from
// now on.
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
        if (newLine>=0 && newLine!=line)
            result.line = newLine;
        result.fingerprint = contentContextFingerprint(content, result.line);
        result.changed = true;
    }
    return result;
}

#endif // BREAKPOINTANCHOR_H
