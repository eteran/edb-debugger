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

#include "OptionsPage.h"
#include <QSettings>

#include "ui_OptionsPage.h"

namespace DumpState {

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
	ui->instructionsBeforeIP->setValue(settings.value("DumpState/instructions_before_ip", 0).toInt());
	ui->instructionsAfterIP->setValue(settings.value("DumpState/instructions_after_ip", 5).toInt());
	ui->colorizeOutput->setChecked(settings.value("DumpState/colorize", true).toBool());
}

//------------------------------------------------------------------------------
// Name: on_instructionsBeforeIP_valueChanged
// Desc:
//------------------------------------------------------------------------------
void OptionsPage::on_instructionsBeforeIP_valueChanged(int i) {
	QSettings settings;
	settings.setValue("DumpState/instructions_before_ip", i);
}

//------------------------------------------------------------------------------
// Name: on_instructionsAfterIP_valueChanged
// Desc:
//------------------------------------------------------------------------------
void OptionsPage::on_instructionsAfterIP_valueChanged(int i) {
	QSettings settings;
	settings.setValue("DumpState/instructions_after_ip", i);
}

//------------------------------------------------------------------------------
// Name: on_colorizeOutput_toggled
// Desc:
//------------------------------------------------------------------------------
void OptionsPage::on_colorizeOutput_toggled(bool value) {
	QSettings settings;
	settings.setValue("DumpState/colorize", value);
}

}
