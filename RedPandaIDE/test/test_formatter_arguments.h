#ifndef TEST_FORMATTER_ARGUMENTS_H
#define TEST_FORMATTER_ARGUMENTS_H
#include <QObject>
#include <memory>

class SettingsPersistor;
class CodeFormatterSettings;

// The command line the formatters are run with (CodeFormatterSettings::
// getAstyleArguments() / getClangFormatArguments()). It is built from the
// settings only, so it is tested without astyle or clang-format being installed:
// a wrong argument is otherwise only visible as an odd formatting result.
class TestFormatterArguments : public QObject
{
    Q_OBJECT
public:
    explicit TestFormatterArguments(QObject *parent=nullptr);
    // out-of-line: the unique_ptr members below need the complete types
    ~TestFormatterArguments() override;
private slots:
    void init();
    void cleanup();

    void test_astyle_defaultBraceStyle();
    void test_astyle_allmanAndIndent();
    void test_astyle_attachFlagsFollowTheSettings();
    void test_clangFormat_configFileStyle();
    void test_clangFormat_styleOverrides();
    void test_clangFormat_extraArguments();
private:
    std::unique_ptr<SettingsPersistor> mPersistor;
    std::unique_ptr<CodeFormatterSettings> mSettings;
};

#endif // TEST_FORMATTER_ARGUMENTS_H
