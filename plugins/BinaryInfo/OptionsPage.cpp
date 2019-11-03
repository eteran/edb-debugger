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
#include "demangle.h"
#include <QFileDialog>
#include <QSettings>

namespace BinaryInfoPlugin {

OptionsPage::OptionsPage(QWidget *parent, Qt::WindowFlags f)
	: QWidget(parent, f) {
	ui.setupUi(this);
}

void OptionsPage::showEvent(QShowEvent *) {

	QSettings settings;

#ifdef DEMANGLING_SUPPORTED
	ui.checkBox->setChecked(settings.value("BinaryInfo/demangling_enabled", true).toBool());
#else
	ui.checkBox->setEnabled(false);
	ui.checkBox->setChecked(false);
#endif

	ui.txtDebugDir->setText(settings.value("BinaryInfo/debug_info_path", "/usr/lib/debug").toString());
}

void OptionsPage::on_checkBox_toggled(bool checked) {
	QSettings settings;
	settings.setValue("BinaryInfo/demangling_enabled", checked);
}

void OptionsPage::on_txtDebugDir_textChanged(const QString &text) {
	QSettings settings;
	settings.setValue("BinaryInfo/debug_info_path", text);
}

void OptionsPage::on_btnDebugDir_clicked() {
	QString dir = QFileDialog::getExistingDirectory(
		this,
		tr("Choose a directory"),
		QString(),
		QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

	if (!dir.isNull()) {
		ui.txtDebugDir->setText(dir);
	}
}

}
