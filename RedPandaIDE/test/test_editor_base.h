#ifndef TEST_EDITOR_BASE_H
#define TEST_EDITOR_BASE_H
#include <QObject>
#include <memory>

class Editor;
class SettingsPersistor;
class DirSettings;
class EditorSettings;
class ColorManager;

class TestEditorBase: public QObject
{
    Q_OBJECT
public:
    TestEditorBase(QObject *parent=nullptr);
protected:
    void init_editor();
    // The editor is shared by all the tests of a class (see init_editor()), so a
    // callback installed by one of them must not be left for the next one: it usually
    // captures the model of the test that installed it, which is destroyed with it
    // (see ContentReplacedFunc).
protected slots:
    void cleanup();

protected:
    std::shared_ptr<Editor> mEditor;
    std::shared_ptr<SettingsPersistor> mSettingsPersistor;
    std::shared_ptr<DirSettings> mDirSettings;
    std::shared_ptr<EditorSettings> mEditorSettings;
    std::shared_ptr<ColorManager> mColorManager;

};

#endif
