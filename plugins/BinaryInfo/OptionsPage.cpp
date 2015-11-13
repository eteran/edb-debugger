/*
Copyright (C) 2015 Ruslan Kabatsayev <b7.10110111@gmail.com>

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
#include "demangle.h"

#include "ui_OptionsPage.h"

namespace BinaryInfo
{

OptionsPage::OptionsPage(QWidget* parent) : QWidget(parent), ui(new Ui::OptionsPage)
{
	ui->setupUi(this);
}

OptionsPage::~OptionsPage()
{
	delete ui;
}

void OptionsPage::showEvent(QShowEvent*)
{
	QSettings settings;
	if(DEMANGLING_SUPPORTED)
		ui->checkBox->setChecked(settings.value("BinaryInfo/demangling_enabled", true).toBool());
	else
	{
		ui->checkBox->setEnabled(false);
		ui->checkBox->setChecked(false);
	}
}

void OptionsPage::on_checkBox_toggled(bool checked)
{
	QSettings().setValue("BinaryInfo/demangling_enabled", checked);
}

}
