/*
 * Copyright (C) 2015 Ruslan Kabatsayev <b7.10110111@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
