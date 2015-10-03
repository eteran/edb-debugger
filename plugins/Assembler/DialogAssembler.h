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

#ifndef DIALOG_ASSEMBLER_20130611_H_
#define DIALOG_ASSEMBLER_20130611_H_

#include <QDialog>
#include "Types.h"
#include "IRegion.h"

namespace Assembler {

namespace Ui { class DialogAssembler; }

class DialogAssembler : public QDialog {
	Q_OBJECT

public:
	DialogAssembler(QWidget *parent = 0);
	virtual ~DialogAssembler();
	
public Q_SLOTS:
	void on_buttonBox_accepted();
	
public:
	void set_address(edb::address_t address);

public:
	virtual void showEvent(QShowEvent *event) override;

private:
	 Ui::DialogAssembler *const ui;
	 edb::address_t             address_;
	 int                        instruction_size_;
};

}

#endif
