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

#ifndef DIALOGBREAKPOINTS_20061101_H_
#define DIALOGBREAKPOINTS_20061101_H_

#include <QDialog>

namespace BreakpointManager {

namespace Ui { class DialogBreakpoints; }

class DialogBreakpoints : public QDialog {
	Q_OBJECT

public:
	DialogBreakpoints(QWidget *parent = 0);
	virtual ~DialogBreakpoints();

public Q_SLOTS:
	void updateList();
	void on_btnAdd_clicked();
	void on_btnRemove_clicked();
	void on_btnCondition_clicked();
	void on_tableWidget_cellDoubleClicked(int row, int col);
    void on_btnImport_clicked();
    void on_btnExport_clicked();

private:
	virtual void showEvent(QShowEvent *event);
	virtual void hideEvent(QHideEvent *event);

private:
	 Ui::DialogBreakpoints *const ui;
};

}

#endif
