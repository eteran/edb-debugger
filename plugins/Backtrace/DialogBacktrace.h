/*
Copyright (C) 2015	Armen Boursalian
					aboursalian@gmail.com

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
