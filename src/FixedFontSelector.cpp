/*
 * Copyright (C) 2014 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "FixedFontSelector.h"
#include <QtDebug>

/**
 * @brief
 */
FixedFontSelector::FixedFontSelector(QWidget *parent, Qt::WindowFlags f)
	: QWidget(parent, f) {

	ui.setupUi(this);

	for (int size : QFontDatabase::standardSizes()) {
		ui.fontSize->addItem(QStringLiteral("%1").arg(size), size);
	}
}

/**
 * @brief
 */
QFont FixedFontSelector::currentFont() const {
	return ui.fontCombo->currentFont();
}

/**
 * @brief
 */
void FixedFontSelector::setCurrentFont(const QString &font) {

	QFont f;
	f.fromString(font);
	setCurrentFont(f);
}

/**
 * @brief
 */
void FixedFontSelector::setCurrentFont(const QFont &font) {
	ui.fontCombo->setCurrentFont(font);
	int n = ui.fontSize->findData(font.pointSize());
	if (n != -1) {
		ui.fontSize->setCurrentIndex(n);
	}
}

/**
 * @brief
 */
void FixedFontSelector::on_fontCombo_currentFontChanged(const QFont &font) {
	Q_EMIT currentFontChanged(font);
}

/**
 * @brief
 */
void FixedFontSelector::on_fontSize_currentIndexChanged(int index) {

	QFont font = ui.fontCombo->currentFont();
	font.setPointSize(ui.fontSize->itemData(index).toInt());
	ui.fontCombo->setCurrentFont(font);

	Q_EMIT currentFontChanged(font);
}
