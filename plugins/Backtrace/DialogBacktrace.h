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

#ifndef DIALOGBACKTRACE_H
#define DIALOGBACKTRACE_H

#include "CallStack.h"

#include <QDialog>
#include <QTableWidget>

namespace Ui {
class DialogBacktrace;
}

class DialogBacktrace : public QDialog
{
	Q_OBJECT

public:
	explicit DialogBacktrace(QWidget *parent = 0);
	~DialogBacktrace();

private:
	bool is_ret(const QTableWidgetItem *item);
	bool is_ret(int column);
	edb::address_t address_from_table(bool *ok, const QTableWidgetItem *item);

public Q_SLOTS:
	void populate_table();

private slots:
	virtual void showEvent(QShowEvent *);
	virtual void resizeEvent(QResizeEvent *);
	virtual void hideEvent(QHideEvent *);
	void on_pushButtonClose_clicked();

	void on_tableWidgetCallStack_itemDoubleClicked(QTableWidgetItem *item);

	void on_tableWidgetCallStack_cellClicked(int row, int column);

	void on_pushButtonReturnTo_clicked();

private:
	Ui::DialogBacktrace		*ui;
	QTableWidget			*table_;
};

#endif // DIALOGBACKTRACE_H
