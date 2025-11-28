/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "OptionsPage.h"
#include <QSettings>

namespace DumpStatePlugin {

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
	ui.instructionsBeforeIP->setValue(settings.value("DumpState/instructions_before_ip", 0).toInt());
	ui.instructionsAfterIP->setValue(settings.value("DumpState/instructions_after_ip", 5).toInt());
	ui.colorizeOutput->setChecked(settings.value("DumpState/colorize", true).toBool());
}

/**
 * @brief OptionsPage::on_instructionsBeforeIP_valueChanged
 * @param i
 */
void OptionsPage::on_instructionsBeforeIP_valueChanged(int i) {
	QSettings settings;
	settings.setValue("DumpState/instructions_before_ip", i);
}

/**
 * @brief OptionsPage::on_instructionsAfterIP_valueChanged
 * @param i
 */
void OptionsPage::on_instructionsAfterIP_valueChanged(int i) {
	QSettings settings;
	settings.setValue("DumpState/instructions_after_ip", i);
}

/**
 * @brief OptionsPage::on_colorizeOutput_toggled
 * @param value
 */
void OptionsPage::on_colorizeOutput_toggled(bool value) {
	QSettings settings;
	settings.setValue("DumpState/colorize", value);
}

}
