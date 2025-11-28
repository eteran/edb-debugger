/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DIALOG_FUNCTIONS_H_20061101_
#define DIALOG_FUNCTIONS_H_20061101_

#include "Types.h"
#include "ui_DialogFunctions.h"
#include <QDialog>

class QSortFilterProxyModel;

namespace FunctionFinderPlugin {

class DialogFunctions : public QDialog {
	Q_OBJECT

public:
	explicit DialogFunctions(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogFunctions() override = default;

private:
	void showEvent(QShowEvent *event) override;

private:
	void doFind();

private:
	Ui::DialogFunctions ui;
	QSortFilterProxyModel *filterModel_ = nullptr;
	QPushButton *buttonFind_            = nullptr;
};

}

#endif
