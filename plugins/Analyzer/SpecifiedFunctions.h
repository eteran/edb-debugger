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

#ifndef DIALOG_SPECIFIED_FUNCTIONS_H_20070705_
#define DIALOG_SPECIFIED_FUNCTIONS_H_20070705_

#include "ui_SpecifiedFunctions.h"
#include <QDialog>

class QStringListModel;
class QSortFilterProxyModel;
class QModelIndex;

namespace AnalyzerPlugin {

class SpecifiedFunctions : public QDialog {
	Q_OBJECT

public:
	explicit SpecifiedFunctions(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~SpecifiedFunctions() override = default;

public Q_SLOTS:
	void on_function_list_doubleClicked(const QModelIndex &index);

private:
	void showEvent(QShowEvent *event) override;

private:
	void doFind();

private:
	Ui::SpecifiedFunctions ui;
	QStringListModel *model_            = nullptr;
	QSortFilterProxyModel *filterModel_ = nullptr;
	QPushButton *buttonRefresh_         = nullptr;
};

}

#endif
