/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DIALOG_BREAKPOINTS_H_20061101_
#define DIALOG_BREAKPOINTS_H_20061101_

#include "ui_DialogBreakpoints.h"
#include <QDialog>

class DialogBreakpoints final : public QDialog {
	Q_OBJECT

public:
	explicit DialogBreakpoints(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogBreakpoints() override = default;

public Q_SLOTS:
	void updateList();
	void on_btnAdd_clicked();
	void on_btnRemove_clicked();
	void on_btnCondition_clicked();
	void on_tableWidget_cellDoubleClicked(int row, int col);
	void on_btnImport_clicked();
	void on_btnExport_clicked();

private:
	void showEvent(QShowEvent *event) override;
	void hideEvent(QHideEvent *event) override;

private:
	Ui::DialogBreakpoints ui;
};

#endif
