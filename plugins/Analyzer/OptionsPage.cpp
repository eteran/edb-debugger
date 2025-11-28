/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "OptionsPage.h"
#include <QSettings>

namespace AnalyzerPlugin {

/**
 * @brief OptionsPage::OptionsPage
 * @param parent
 * @param f
 */
OptionsPage::OptionsPage(QWidget *parent, Qt::WindowFlags f)
	: QWidget(parent, f) {

	ui.setupUi(this);
	connect(ui.checkBox, &QCheckBox::toggled, this, &OptionsPage::checkBoxToggled);
}

/**
 * @brief OptionsPage::showEvent
 * @param event
 */
void OptionsPage::showEvent(QShowEvent *event) {
	Q_UNUSED(event)

	QSettings settings;
	ui.checkBox->setChecked(settings.value("Analyzer/fuzzy_logic_functions.enabled", true).toBool());
}

/**
 * @brief OptionsPage::checkBoxToggled
 * @param checked
 */
void OptionsPage::checkBoxToggled(bool checked) {
	Q_UNUSED(checked)

	QSettings settings;
	settings.setValue("Analyzer/fuzzy_logic_functions.enabled", ui.checkBox->isChecked());
}

}
