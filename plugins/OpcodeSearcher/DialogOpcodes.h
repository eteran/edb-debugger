/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DIALOG_OPCODES_H_20061101_
#define DIALOG_OPCODES_H_20061101_

#include "ui_DialogOpcodes.h"
#include <QDialog>

class QSortFilterProxyModel;

namespace OpcodeSearcherPlugin {

class DialogResults;

class DialogOpcodes : public QDialog {
	Q_OBJECT

public:
	explicit DialogOpcodes(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogOpcodes() override = default;

private:
	void doFind();

private:
	void showEvent(QShowEvent *event) override;

private:
	Ui::DialogOpcodes ui;
	QSortFilterProxyModel *filterModel_ = nullptr;
	QPushButton *buttonFind_            = nullptr;
};

}

#endif
