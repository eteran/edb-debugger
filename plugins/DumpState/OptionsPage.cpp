/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "OptionsPage.h"
#include <QSettings>

namespace DumpStatePlugin {

/**
 * @brief Constructor for the OptionsPage class, which is a QWidget that provides an interface for configuring the settings of the DumpState plugin.
 *
 * @param parent The parent widget for this options page.
 * @param f The window flags for this options page.
 */
OptionsPage::OptionsPage(QWidget *parent, Qt::WindowFlags f)
	: QWidget(parent, f) {

	ui.setupUi(this);
}

/**
 * @brief Handler for the show event of the options page. This function loads the current settings for the plugin and updates the UI elements to reflect those settings when the options page is shown.
 *
 * @param event The show event that triggered this handler.
 */
void OptionsPage::showEvent(QShowEvent * /*event*/) {

	QSettings settings;
	ui.instructionsBeforeIP->setValue(settings.value(QStringLiteral("DumpState/instructions_before_ip"), 0).toInt());
	ui.instructionsAfterIP->setValue(settings.value(QStringLiteral("DumpState/instructions_after_ip"), 5).toInt());
	ui.colorizeOutput->setChecked(settings.value(QStringLiteral("DumpState/colorize"), true).toBool());
}

/**
 * @brief Handler for the valueChanged signal of the instructionsBeforeIP spin box. This function updates the corresponding setting for the plugin when the user changes the value in the UI.
 *
 * @param i The new value of the spin box.
 */
void OptionsPage::on_instructionsBeforeIP_valueChanged(int i) {
	QSettings settings;
	settings.setValue(QStringLiteral("DumpState/instructions_before_ip"), i);
}

/**
 * @brief Handler for the valueChanged signal of the instructionsAfterIP spin box. This function updates the corresponding setting for the plugin when the user changes the value in the UI.
 *
 * @param i The new value of the spin box.
 */
void OptionsPage::on_instructionsAfterIP_valueChanged(int i) {
	QSettings settings;
	settings.setValue(QStringLiteral("DumpState/instructions_after_ip"), i);
}

/**
 * @brief Handler for the toggled signal of the colorizeOutput check box. This function updates the corresponding setting for the plugin when the user changes the state of the check box.
 *
 * @param value The new state of the check box.
 */
void OptionsPage::on_colorizeOutput_toggled(bool value) {
	QSettings settings;
	settings.setValue(QStringLiteral("DumpState/colorize"), value);
}

}
