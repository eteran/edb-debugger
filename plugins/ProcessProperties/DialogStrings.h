/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DIALOG_STRINGS_H_20061101_
#define DIALOG_STRINGS_H_20061101_

#include "Types.h"
#include "ui_DialogStrings.h"
#include <QDialog>

class QSortFilterProxyModel;
class QListWidgetItem;

namespace ProcessPropertiesPlugin {

class DialogStrings : public QDialog {
	Q_OBJECT

public:
	explicit DialogStrings(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogStrings() override = default;

private:
	void showEvent(QShowEvent *event) override;

private:
	void doFind();

private:
	Ui::DialogStrings ui;
	QSortFilterProxyModel *filterModel_ = nullptr;
	QPushButton *buttonFind_            = nullptr;
};

}

#endif
