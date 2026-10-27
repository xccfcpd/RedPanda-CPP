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
#ifndef FORMATTERGENERALWIDGET_H
#define FORMATTERGENERALWIDGET_H

#include <QAbstractListModel>
#include <QComboBox>
#include <QThreadPool>
#include <QTimer>
#include <QWidget>
#include "settingswidget.h"
#include "../utils.h"
#include "../settings.h"

namespace Ui {
class FormatterGeneralWidget;
}

struct FormatterStyleItem {
    QString name;
    QString description;
    FormatterBraceStyle style;
    explicit FormatterStyleItem(const QString& name,
                                const QString& description,
                                FormatterBraceStyle style);
};
using PFormatterStyleItem = std::shared_ptr<FormatterStyleItem>;

class FormatterStyleModel : public QAbstractListModel{
    Q_OBJECT
    // QAbstractItemModel interface
public:
    explicit FormatterStyleModel(QObject *parent=nullptr);
    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    PFormatterStyleItem getStyle(const QModelIndex &index);
    PFormatterStyleItem getStyle(int index);
private:
    QList<PFormatterStyleItem> mStyles;
};

class QShowEvent;
class ColorManager;
class FormatterGeneralWidget : public SettingsWidget
{
    Q_OBJECT

public:
    explicit FormatterGeneralWidget(ColorManager *colorManager, const QString& name, const QString& group,IconsManager *iconsManager,  QWidget *parent = nullptr);
    ~FormatterGeneralWidget();
private slots:
    void onBraceStyleChanged();

    void on_chkBreakMaxCodeLength_stateChanged(int arg1);

    void on_clangFormatStyleChanged();

    void on_chkClangFormatOverrideStyle_toggled(bool checked);

    void on_chkClangFormatUseFallbackStyle_toggled(bool checked);

    void on_chkClangFormatSetColumnLimit_toggled(bool checked);

    void updateDemo();
private:
    void updateCodeFormatter(CodeFormatterSettings& format);
    void updateClangFormat(CodeFormatterSettings& format);
    void updateFormatterEngine();
    void updateClangFormatStyleDescription();
    void setupClangFormatComboboxes();
    int findClangFormatStyleIndex(QComboBox *comboBox, int value, int fallbackValue) const;
    // Applies the result of the asynchronous demo to the widget, unless a newer
    // demo has been requested in the meantime.
    void applyDemoResult(quint64 generation, const QString& display);

private:
    Ui::FormatterGeneralWidget *ui;
    FormatterStyleModel mStylesModel;
    QTimer mDemoTimer;
    quint64 mDemoGeneration = 0;

    // SettingsWidget interface
protected:
    void doLoad() override;
    void doSave() override;
    // The demo runs a formatter process: start it when the page is shown, not when
    // the settings dialog loads all of its pages.
    void showEvent(QShowEvent *event) override;

    // SettingsWidget interface
};

#endif // FORMATTERGENERALWIDGET_H
