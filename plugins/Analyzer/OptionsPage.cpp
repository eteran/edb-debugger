/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "OptionsPage.h"
#include <QSettings>

namespace AnalyzerPlugin {

/**
 * @brief Constructs the Analyzer options page and connects the fuzzy-logic checkbox to its handler.
 *
 * @param parent
 * @param f
 */
OptionsPage::OptionsPage(QWidget *parent, Qt::WindowFlags f)
	: QWidget(parent, f) {

	ui.setupUi(this);
	connect(ui.checkBox, &QCheckBox::toggled, this, &OptionsPage::checkBoxToggled);
}

/**
 * @brief Populates the options page controls from saved settings when it becomes visible.
 *
 * @param event
 */
void OptionsPage::showEvent(QShowEvent *event) {
	Q_UNUSED(event)

	QSettings settings;
	ui.checkBox->setChecked(settings.value(QStringLiteral("Analyzer/fuzzy_logic_functions.enabled"), true).toBool());
}

/**
 * @brief Persists the fuzzy-logic functions enabled setting whenever the checkbox state changes.
 *
 * @param checked
 */
void OptionsPage::checkBoxToggled(bool checked) {
	Q_UNUSED(checked)

	QSettings settings;
	settings.setValue(QStringLiteral("Analyzer/fuzzy_logic_functions.enabled"), ui.checkBox->isChecked());
}

}
