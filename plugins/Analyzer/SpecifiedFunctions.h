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

#ifndef DIALOGSPECIFIED_FUNCTIONS_20070705_H_
#define DIALOGSPECIFIED_FUNCTIONS_20070705_H_

#include <QDialog>

class QStringListModel;
class QSortFilterProxyModel;
class QModelIndex;

namespace Analyzer {

namespace Ui { class SpecifiedFunctions; }

class SpecifiedFunctions : public QDialog {
	Q_OBJECT

public:
	SpecifiedFunctions(QWidget *parent = 0);
	virtual ~SpecifiedFunctions();

public Q_SLOTS:
	void on_function_list_doubleClicked(const QModelIndex &index);
	void on_refresh_button_clicked();

private:
	virtual void showEvent(QShowEvent *event);

private:
	void do_find();

private:
	 Ui::SpecifiedFunctions *const ui;
	 QStringListModel *         model_;
	 QSortFilterProxyModel *    filter_model_;
};

}

#endif
