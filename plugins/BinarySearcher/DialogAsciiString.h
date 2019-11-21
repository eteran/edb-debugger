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

#ifndef DIALOG_ASCII_STRING_H_20082201_
#define DIALOG_ASCII_STRING_H_20082201_

#include "ui_DialogAsciiString.h"
#include <QDialog>

class QListWidgetItem;

namespace BinarySearcherPlugin {

class DialogAsciiString : public QDialog {
	Q_OBJECT

public:
	explicit DialogAsciiString(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogAsciiString() override = default;

protected:
	void showEvent(QShowEvent *event) override;

private:
	void doFind();

private:
	Ui::DialogAsciiString ui;
	QPushButton *buttonFind_ = nullptr;
};

}

#endif
