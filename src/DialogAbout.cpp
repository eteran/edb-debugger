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

#include "DialogAbout.h"
#include "ui_DialogAbout.h"
#include "version.h"

//------------------------------------------------------------------------------
// Name: DialogAbout
// Desc: constructor
//------------------------------------------------------------------------------
DialogAbout::DialogAbout(QWidget *parent) : QDialog(parent), ui(new Ui::DialogAbout) {
	ui->setupUi(this);
	ui->labelVersion->setText(tr("Version: %1\nCompiled: %2").arg(edb::version).arg(__DATE__));
}

//------------------------------------------------------------------------------
// Name: ~DialogAbout
// Desc:
//------------------------------------------------------------------------------
DialogAbout::~DialogAbout() {
	delete ui;
}
