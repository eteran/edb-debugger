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

#ifndef DIALOG_STRINGS_H_20061101_
#define DIALOG_STRINGS_H_20061101_

#include "Types.h"
#include "ui_DialogStrings.h"
#include <QDialog>

class QSortFilterProxyModel;
class QListWidgetItem;

namespace ProcessPropertiesPlugin {

class DialogStrings : public QDialog {
	Q_OBJECT

public:
	explicit DialogStrings(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogStrings() override = default;

private:
	void showEvent(QShowEvent *event) override;

private:
	void doFind();

private:
	Ui::DialogStrings ui;
	QSortFilterProxyModel *filterModel_ = nullptr;
	QPushButton *buttonFind_            = nullptr;
};

}

#endif
