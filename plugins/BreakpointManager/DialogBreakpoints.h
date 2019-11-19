/*
Copyright (C) 2006 - 2015 Evan Teran
                          evan.teran@gmail.com

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

#ifndef DIALOG_BREAKPOINTS_H_20061101_
#define DIALOG_BREAKPOINTS_H_20061101_

#include "ui_DialogBreakpoints.h"
#include <QDialog>

namespace BreakpointManagerPlugin {

class DialogBreakpoints : public QDialog {
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

}

#endif
