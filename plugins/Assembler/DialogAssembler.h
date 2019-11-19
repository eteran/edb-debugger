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

#ifndef DIALOG_ASSEMBLER_H_20130611_
#define DIALOG_ASSEMBLER_H_20130611_

#include "IRegion.h"
#include "Types.h"
#include "ui_DialogAssembler.h"
#include <QDialog>

namespace AssemblerPlugin {

class DialogAssembler : public QDialog {
	Q_OBJECT

public:
	explicit DialogAssembler(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogAssembler() override = default;

public Q_SLOTS:
	void on_buttonBox_accepted();

public:
	void setAddress(edb::address_t address);

public:
	void showEvent(QShowEvent *event) override;

private:
	Ui::DialogAssembler ui;
	edb::address_t address_ = 0;
	size_t instructionSize_ = 0;
};

}

#endif
