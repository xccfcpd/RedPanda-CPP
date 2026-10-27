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
#ifndef EDITORMANAGER_H
#define EDITORMANAGER_H

#include <QTabWidget>
#include <QSplitter>
#include <QWidget>
#include <QRecursiveMutex>
#include <QHash>
#include <QScopedValueRollback>
#include "utils.h"
#include "editor.h"
#include "reformatter/astyleformatter.h"
#include "reformatter/clangformatreformatter.h"
#include "debugger/debuggermodels.h"

class MainWindow;
class Project;
class EditorManager : public QObject
{
    Q_OBJECT
public:
    enum class LayoutShowType{
        lstLeft,
        lstRight,
        lstBoth
    };

    explicit EditorManager(QTabWidget* leftPageWidget,
                        QTabWidget* rightPageWidget,
                        QSplitter* splitter,
                        QWidget* panel, QObject* parent = nullptr);

    Editor* newEditor(const QString& filename, const QByteArray& encoding,
                      FileType fileType, const QString& contextFile,
                     bool inProject, bool newFile,
                     QTabWidget* page=nullptr);

    Editor* getEditor(int index=-1, QTabWidget* tabsWidget=nullptr) const;

    bool closeEditor(Editor* editor, bool transferFocus=true, bool force=false);

    bool swapEditor(Editor* editor);
    void activeEditor(Editor *e, bool focus);
    void activeEditorAndSetCaret(Editor *e, QSynedit::CharPos pos);

    void saveAll();
    bool saveAllForProject();

    bool projectEditorsModified();
    void clearProjectEditorsModified();

    // Application-level undo/redo of the breakpoint toggles (see
    // Editor::setBreakpointHistoryFuncs()).
    struct BreakpointHistoryAction {
        enum Kind { Added, Removed, ClearedAll };
        Kind kind = Added;
        PBreakpoint breakpoint;
        QList<PBreakpoint> breakpoints;
        bool forProject = false;
    };
    QHash<const Editor*, QList<BreakpointHistoryAction>> mBreakpointUndo;
    QHash<const Editor*, QList<BreakpointHistoryAction>> mBreakpointRedo;
    bool mRestoringBreakpointAction = false;
    // The last breakpoint history action belongs to the table (no open editor).
    bool mLastBreakpointActionWasUnbound = false;
    bool undoBreakpointAction(Editor* e);
    bool redoBreakpointAction(Editor* e);
    bool canUndoBreakpointAction(const Editor* e) const;
    bool canRedoBreakpointAction(const Editor* e) const;
    void recordBreakpointAction(const Editor* e, bool added,
                                const PBreakpoint& breakpoint, bool forProject);
    void clearBreakpointHistory(const Editor* e);

    bool closeAll(bool force = false);
    bool closeOthers(Editor* editor);

    void forceCloseEditor(Editor* editor);

    Editor* getOpenedEditor(const QString &filename) const;

    bool getContentFromOpenedEditor(const QString& filename, QStringList& buffer) const;

    void getVisibleEditors(Editor*& left, Editor*& right) const;
    void updateLayout();

    MainWindow * mainWindow();


    QTabWidget *findPageControlForEditor(Editor *e);

    void updateEditorBookmarks();
    void updateEditorBreakpoints();
    // Remove the breakpoint selected in the breakpoint view and keep it in the
    // application-level undo history of its editor (if that file is open).
    bool removeBreakpointFromTable(int index, bool forProject);
    // Remove all the breakpoints shown in the table as one undoable action.
    void removeAllBreakpointsFromTable(bool forProject);
    // Undo/redo of the breakpoints removed from the breakpoint table while their
    // file has no open editor. There is no Editor to hold the command, so they
    // use the null key of the same history.
    bool canUndoUnboundBreakpoint() const;
    bool canRedoUnboundBreakpoint() const;
    bool lastBreakpointActionWasUnbound() const;
    bool undoUnboundBreakpoint();
    bool redoUnboundBreakpoint();
    // Drop the application-level breakpoint histories (project switch, clear all).
    void clearBreakpointHistories();

    bool debuggerReadyForEvalTip();
    bool requestEvalTip(Editor *e, const QString& s);
    void onEditorTipEvalValueReady(Editor *e);

    void beginUpdate();
    void endUpdate();
    void applySettings();
    void applyColorSchemes(const QString& name);
    bool isFileOpened(const QString& fullfilepath) const;
    bool hasFilename(const QString& filename) const;
    int pageCount() const;
    void selectNextPage();
    void selectPreviousPage();
    void showActiveEditorCaret();

    Editor* operator[](int index);

    QTabWidget *leftPageWidget() const;

    QTabWidget *rightPageWidget() const;

    PCppParser sharedParser(ParserLanguage language, const QTabWidget *widget);
    void resetSharedParsers();

    PCppParser createParserForEditor(Editor *editor);

    std::unique_ptr<BaseReformatter> createReformatterForEditor(Editor *);

signals:
    void editorClosed();
    void editorOpenned();
private:
    PCppParser sharedParser(QHash<ParserLanguage,std::weak_ptr<CppParser>> & sharedParsers, ParserLanguage language);
    void resetSharedParsers(QHash<ParserLanguage,std::weak_ptr<CppParser>> & sharedParsers);

    QTabWidget* getNewEditorPageControl() const;
    QTabWidget* getFocusedPageControl() const;
    void showLayout(LayoutShowType layout);
    void doRemoveEditor(Editor* e);
#ifdef ENABLE_SDCC
    CompilerType getCompilerTypeForEditor(const Editor *e) const;
#endif
private slots:
    void updateEditorTabCaption(Editor* e);
    void onBreakpointAdded(const Editor* e, int line, const QString& fingerprint);
    void onBreakpointRemoved(const Editor* e, int line);
    void onBreakpointsCleared(const Editor* e);
    void onEditorShown(Editor *e);
    void onFileSaving(Editor *e, const QString& filename);
    void onFileSaved(Editor *e, const QString& filename);
    void onFileRenamed(Editor *e, const QString &oldFilename, const QString &newFilename);
    void onFileSaveAsed(Editor *e, const QString &oldFilename, const QString &newFilename);
    void onFileSaveError(Editor *e, const QString& filename, const QString& reason);
    void onEditorLinesInserted(int startLine, int count);
    void onEditorLinesRemoved(int startLine, int count);
    void onEditorLineMoved(int fromLine, int toLine);
    void onEditorLinesMerged(int removedLine, int intoLine);
    void onEditorLinesSplit(int mergedLine, int newLine);
    void onEditorStatusChanged(QSynedit::StatusChanges changes);
    void onEditorFontSizeChangedByWheel(int newSize);
    void onEditorFileEncodingChanged(Editor *e);
private:
    LayoutShowType mLayout;
    QTabWidget *mLeftPageWidget;
    QTabWidget *mRightPageWidget;
    QSplitter *mSplitter;
    QWidget *mPanel;
    int mUpdateCount;
    QHash<ParserLanguage,std::weak_ptr<CppParser>> mSharedParsers;
    QHash<ParserLanguage,std::weak_ptr<CppParser>> mSharedParsersForRight;
    QHash<ParserLanguage,std::weak_ptr<CppParser>> mSharedParsersForLeft;
    mutable QRecursiveMutex mMutex;
};

#endif // EDITORLIST_H
