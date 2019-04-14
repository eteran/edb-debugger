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

namespace AnalyzerPlugin {

//------------------------------------------------------------------------------
// Name: OptionsPage
// Desc:
//------------------------------------------------------------------------------
OptionsPage::OptionsPage(QWidget *parent, Qt::WindowFlags f) : QWidget(parent, f)  {
	ui.setupUi(this);
	connect(ui.checkBox, &QCheckBox::toggled, this, &OptionsPage::checkBox_toggled);
}


//------------------------------------------------------------------------------
// Name: showEvent
// Desc:
//------------------------------------------------------------------------------
void OptionsPage::showEvent(QShowEvent *event) {
	Q_UNUSED(event)

	QSettings settings;
	ui.checkBox->setChecked(settings.value("Analyzer/fuzzy_logic_functions.enabled", true).toBool());
}

//------------------------------------------------------------------------------
// Name: checkBox_toggled
// Desc:
//------------------------------------------------------------------------------
void OptionsPage::checkBox_toggled(bool checked) {
	Q_UNUSED(checked)

	QSettings settings;
	settings.setValue("Analyzer/fuzzy_logic_functions.enabled", ui.checkBox->isChecked());
}

}
