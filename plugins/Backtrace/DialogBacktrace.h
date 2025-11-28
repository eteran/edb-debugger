/*
 * Copyright (C) 2015 Armen Boursalian <aboursalian@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DIALOG_BACKTRACE_H_20191119_
#define DIALOG_BACKTRACE_H_20191119_

#include "CallStack.h"
#include "ui_DialogBacktrace.h"
#include <QDialog>
#include <QTableWidget>

namespace BacktracePlugin {

class DialogBacktrace : public QDialog {
	Q_OBJECT

public:
	explicit DialogBacktrace(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogBacktrace() override = default;

protected:
	void showEvent(QShowEvent *) override;
	void hideEvent(QHideEvent *) override;

public Q_SLOTS:
	void populateTable();

private Q_SLOTS:
	void on_tableWidgetCallStack_itemDoubleClicked(QTableWidgetItem *item);
	void on_tableWidgetCallStack_cellClicked(int row, int column);

private:
	Ui::DialogBacktrace ui;
	QTableWidget *table_;
	QPushButton *buttonReturnTo_;
};

}

#endif
