/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "OptionsPage.h"
#include <QSettings>

namespace CheckVersionPlugin {

/**
 * @brief OptionsPage::OptionsPage
 * @param parent
 * @param f
 */
OptionsPage::OptionsPage(QWidget *parent, Qt::WindowFlags f)
	: QWidget(parent, f) {

	ui.setupUi(this);
}

/**
 * @brief OptionsPage::showEvent
 * @param event
 */
void OptionsPage::showEvent(QShowEvent *event) {
	Q_UNUSED(event)

	QSettings settings;
	ui.checkBox->setChecked(settings.value("CheckVersion/check_on_start.enabled", true).toBool());
}

/**
 * @brief OptionsPage::on_checkBox_toggled
 * @param checked
 */
void OptionsPage::on_checkBox_toggled(bool checked) {
	Q_UNUSED(checked)

	QSettings settings;
	settings.setValue("CheckVersion/check_on_start.enabled", ui.checkBox->isChecked());
}

}
