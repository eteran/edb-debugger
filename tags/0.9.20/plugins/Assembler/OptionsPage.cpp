/*
Copyright (C) 2006 - 2014 Evan Teran
                          eteran@alum.rit.edu

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

#include "OptionsPage.h"
#include <QSettings>
#include <QDebug>
#include <QFileDialog>

#include "ui_OptionsPage.h"

namespace Assembler {

//------------------------------------------------------------------------------
// Name: OptionsPage
// Desc:
//------------------------------------------------------------------------------
OptionsPage::OptionsPage(QWidget *parent) : QWidget(parent), ui(new Ui::OptionsPage) {
	ui->setupUi(this);
}

//------------------------------------------------------------------------------
// Name: ~OptionsPage
// Desc:
//------------------------------------------------------------------------------
OptionsPage::~OptionsPage() {
	delete ui;
}

//------------------------------------------------------------------------------
// Name: showEvent
// Desc:
//------------------------------------------------------------------------------
void OptionsPage::showEvent(QShowEvent *event) {
	Q_UNUSED(event);

	QSettings settings;
	ui->assemblerPath->setEditText(settings.value("Assembler/helper_application", "/usr/bin/yasm").toString());
}

//------------------------------------------------------------------------------
// Name: on_assemblerPath_editTextChanged
// Desc:
//------------------------------------------------------------------------------
void OptionsPage::on_assemblerPath_editTextChanged(const QString &text) {
	QSettings settings;
	settings.setValue("Assembler/helper_application", text);
}

//------------------------------------------------------------------------------
// Name: on_toolButton_clicked
// Desc:
//------------------------------------------------------------------------------
void OptionsPage::on_toolButton_clicked() {
	const QString filename = QFileDialog::getOpenFileName(this, "Choose Your Preferred Assembler");
	if(!filename.isEmpty()) {
		ui->assemblerPath->setEditText(filename);
	}
}

}
