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

#ifndef DIALOGASCIISTRING_20082201_H_
#define DIALOGASCIISTRING_20082201_H_

#include "ui_DialogASCIIString.h"
#include <QDialog>

class QListWidgetItem;

namespace BinarySearcherPlugin {

class DialogASCIIString : public QDialog {
	Q_OBJECT

public:
	explicit DialogASCIIString(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogASCIIString() override = default;

protected:
	void showEvent(QShowEvent *event) override;

private:
	void do_find();

private:
	 Ui::DialogASCIIString ui;
	 QPushButton *btnFind_;
};

}

#endif
