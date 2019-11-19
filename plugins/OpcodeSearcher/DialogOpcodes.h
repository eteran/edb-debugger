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
