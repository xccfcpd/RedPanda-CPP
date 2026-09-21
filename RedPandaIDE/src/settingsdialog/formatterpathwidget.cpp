#include "formatterpathwidget.h"
#include "ui_formatterpathwidget.h"
#include "../iconsmanager.h"
#include "../settings.h"
#include "../systemconsts.h"

#include <QFileDialog>

FormatterPathWidget::FormatterPathWidget(const QString &name, const QString &group, IconsManager *iconsManager, QWidget *parent) :
    SettingsWidget(name,group,iconsManager,parent),
    ui(new Ui::FormatterPathWidget)
{
    ui->setupUi(this);
}

FormatterPathWidget::~FormatterPathWidget()
{
    delete ui;
}

void FormatterPathWidget::doLoad()
{
    ui->txtAstyle->setText(pSettings->environment().AStylePath());
    ui->txtClangFormat->setText(pSettings->environment().clangFormatPath());
}

void FormatterPathWidget::doSave()
{
    pSettings->environment().setAStylePath(ui->txtAstyle->text());
    pSettings->environment().setClangFormatPath(ui->txtClangFormat->text());
    pSettings->environment().save();
}

void FormatterPathWidget::updateIcons(const QSize &/*size*/)
{
    iconsManager()->setIcon(ui->btnChooseAstyle, IconsManager::ACTION_FILE_OPEN_FOLDER);
    iconsManager()->setIcon(ui->btnChooseClangFormat, IconsManager::ACTION_FILE_OPEN_FOLDER);
}

void FormatterPathWidget::on_btnChooseAstyle_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(
                this,
                tr("Path to astyle"),
                QString(),
                tr("All files (%1)").arg(ALL_FILE_WILDCARD));
    if (!fileName.isEmpty() ) {
        ui->txtAstyle->setText(fileName);
    }
}

void FormatterPathWidget::on_btnChooseClangFormat_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(
                this,
                tr("Path to clang-format"),
                QString(),
                tr("All files (%1)").arg(ALL_FILE_WILDCARD));
    if (!fileName.isEmpty() ) {
        ui->txtClangFormat->setText(fileName);
    }
}
