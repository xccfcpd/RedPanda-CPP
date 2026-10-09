#include "test_formatter_arguments.h"

#include "src/settings/basesettings.h"
#include "src/settings/codeformattersettings.h"
#include "src/utils/types.h"

#include <QDir>
#include <QFile>
#include <QTest>

TestFormatterArguments::TestFormatterArguments(QObject *parent):
    QObject{parent}
{
}

TestFormatterArguments::~TestFormatterArguments() = default;

void TestFormatterArguments::init()
{
    // A dedicated file in the temp dir. The settings are never saved, so the
    // load() below only ever pulls the defaults of CodeFormatterSettings: the
    // tests then change single settings on top of those defaults.
    mPersistor = std::make_unique<SettingsPersistor>(
        QDir::temp().filePath(QStringLiteral("redpanda-test-formatter.ini")));
    mSettings = std::make_unique<CodeFormatterSettings>(mPersistor.get());
    mSettings->load();
}

void TestFormatterArguments::cleanup()
{
    // the file must be gone before the persistor (and its QSettings) is destroyed
    mSettings.reset();
    mPersistor.reset();
    QFile::remove(QDir::temp().filePath(QStringLiteral("redpanda-test-formatter.ini")));
}

void TestFormatterArguments::test_astyle_defaultBraceStyle()
{
    mSettings->setBraceStyle(FormatterBraceStyle::fbsDefault);
    QStringList args = mSettings->getAstyleArguments();
    // "default" means "let astyle choose": no style option at all is emitted
    foreach (const QString &arg, args)
        QVERIFY(!arg.startsWith(QStringLiteral("--style=")));
    // the mandatory options are still there
    QVERIFY(args.contains(QStringLiteral("-I")));
}

void TestFormatterArguments::test_astyle_allmanAndIndent()
{
    mSettings->setBraceStyle(FormatterBraceStyle::fbsAllman);
    mSettings->setIndentStyle(FormatterIndentType::fitSpace);
    mSettings->setTabWidth(3);
    QStringList args = mSettings->getAstyleArguments();
    QVERIFY(args.contains(QStringLiteral("--style=allman")));
    QVERIFY(args.contains(QStringLiteral("--indent=spaces=3")));

    // the indent option follows the indent style and the tab width
    mSettings->setIndentStyle(FormatterIndentType::fitTab);
    mSettings->setTabWidth(8);
    args = mSettings->getAstyleArguments();
    QVERIFY(args.contains(QStringLiteral("--indent=tab=8")));
}

void TestFormatterArguments::test_astyle_attachFlagsFollowTheSettings()
{
    mSettings->setAttachNamespaces(true);
    mSettings->setAttachClasses(true);
    mSettings->setAttachInlines(false);
    mSettings->setAttachExternC(true);
    mSettings->setAttachClosingWhile(false);
    QStringList args = mSettings->getAstyleArguments();
    QVERIFY(args.contains(QStringLiteral("--attach-namespaces")));
    QVERIFY(args.contains(QStringLiteral("--attach-classes")));
    QVERIFY(!args.contains(QStringLiteral("--attach-inlines")));
    QVERIFY(args.contains(QStringLiteral("--attach-extern-c")));
    QVERIFY(!args.contains(QStringLiteral("--attach-closing-while")));
}

void TestFormatterArguments::test_clangFormat_configFileStyle()
{
    mSettings->setClangFormatStyle(ClangFormatStyle::cfsFile);
    mSettings->setClangFormatUseFallbackStyle(true);
    mSettings->setClangFormatFallbackStyle(ClangFormatStyle::cfsLLVM);
    QStringList args = mSettings->getClangFormatArguments();
    QVERIFY(args.contains(QStringLiteral("-style=file")));
    QVERIFY(args.contains(QStringLiteral("-fallback-style=LLVM")));

    // the fallback is only meaningful when the main style is a config file
    mSettings->setClangFormatStyle(ClangFormatStyle::cfsGoogle);
    args = mSettings->getClangFormatArguments();
    QVERIFY(args.contains(QStringLiteral("-style=Google")));
    QVERIFY(!args.contains(QStringLiteral("-fallback-style=LLVM")));

    // getArguments() dispatches to the selected engine
    mSettings->setFormatterEngine(FormatterEngine::feClangFormat);
    QCOMPARE(mSettings->getArguments(), mSettings->getClangFormatArguments());
    mSettings->setFormatterEngine(FormatterEngine::feAStyle);
    QCOMPARE(mSettings->getArguments(), mSettings->getAstyleArguments());
}

void TestFormatterArguments::test_clangFormat_styleOverrides()
{
    mSettings->setClangFormatStyle(ClangFormatStyle::cfsLLVM);
    mSettings->setClangFormatOverrideStyle(true);
    mSettings->setClangFormatIndentWidth(2);
    mSettings->setClangFormatUseTab(ClangFormatUseTab::cfuAlways);
    mSettings->setClangFormatTabWidth(8);
    mSettings->setClangFormatSetColumnLimit(true);
    mSettings->setClangFormatColumnLimit(100);
    mSettings->setClangFormatSortIncludes(true);
    mSettings->setClangFormatAlignConsecutiveAssignments(true);
    QStringList args = mSettings->getClangFormatArguments();
    QVERIFY(args.contains(QStringLiteral(
        "-style={BasedOnStyle: LLVM, IndentWidth: 2, UseTab: Always, TabWidth: 8, "
        "ColumnLimit: 100, SortIncludes: true, AlignConsecutiveAssignments: true}")));

    // the column limit is only written when it is explicitly enabled
    mSettings->setClangFormatSetColumnLimit(false);
    args = mSettings->getClangFormatArguments();
    QVERIFY(!args.first().contains(QStringLiteral("ColumnLimit")));
}

void TestFormatterArguments::test_clangFormat_extraArguments()
{
    mSettings->setClangFormatStyle(ClangFormatStyle::cfsFile);
    mSettings->setClangFormatExtraArguments(
        QStringLiteral("--verbose -sort-includes \"two words\""));
    QStringList args = mSettings->getClangFormatArguments();
    QVERIFY(args.contains(QStringLiteral("--verbose")));
    QVERIFY(args.contains(QStringLiteral("-sort-includes")));
    QVERIFY(args.contains(QStringLiteral("two words")));

    // no extra argument is appended when the field is empty
    mSettings->setClangFormatExtraArguments(QString());
    QCOMPARE(mSettings->getClangFormatArguments(), QStringList{QStringLiteral("-style=file")});
}
