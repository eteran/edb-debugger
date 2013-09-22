/*
Copyright (C) 2006 - 2013 Evan Teran
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

#include "AssemblerOptionsPage.h"
#include <QSettings>
#include <QDebug>
#include <QFileDialog>

#include "ui_AssemblerOptionsPage.h"

//------------------------------------------------------------------------------
// Name: AssemblerOptionsPage
// Desc:
//------------------------------------------------------------------------------
AssemblerOptionsPage::AssemblerOptionsPage(QWidget *parent) : QWidget(parent), ui(new Ui::AssemblerOptionsPage) {
	ui->setupUi(this);
}

//------------------------------------------------------------------------------
// Name: ~AssemblerOptionsPage
// Desc:
//------------------------------------------------------------------------------
AssemblerOptionsPage::~AssemblerOptionsPage() {
	delete ui;
}

//------------------------------------------------------------------------------
// Name: showEvent
// Desc:
//------------------------------------------------------------------------------
void AssemblerOptionsPage::showEvent(QShowEvent *event) {
	Q_UNUSED(event);

	QSettings settings;
	ui->assemblerPath->setEditText(settings.value("Assembler/helper_application", "/usr/bin/yasm").toString());
}

//------------------------------------------------------------------------------
// Name: on_assemblerPath_editTextChanged
// Desc:
//------------------------------------------------------------------------------
void AssemblerOptionsPage::on_assemblerPath_editTextChanged(const QString &text) {
	QSettings settings;
	settings.setValue("Assembler/helper_application", text);
}

//------------------------------------------------------------------------------
// Name: on_toolButton_clicked
// Desc:
//------------------------------------------------------------------------------
void AssemblerOptionsPage::on_toolButton_clicked() {
	const QString filename = QFileDialog::getOpenFileName(this, "Choose Your Preferred Assembler");
	if(!filename.isEmpty()) {
		ui->assemblerPath->setEditText(filename);
	}
}
