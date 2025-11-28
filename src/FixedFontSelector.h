/*
 * Copyright (C) 2014 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef FIXED_FONT_SELECTOR_H_20191119_
#define FIXED_FONT_SELECTOR_H_20191119_

#include <QWidget>

#include "ui_FixedFontSelector.h"

class FixedFontSelector : public QWidget {
	Q_OBJECT

public:
	explicit FixedFontSelector(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~FixedFontSelector() override = default;

public:
	[[nodiscard]] QFont currentFont() const;

public Q_SLOTS:
	void setCurrentFont(const QFont &font);
	void setCurrentFont(const QString &font);

private Q_SLOTS:
	void on_fontCombo_currentFontChanged(const QFont &font);
	void on_fontSize_currentIndexChanged(int index);

Q_SIGNALS:
	void currentFontChanged(const QFont &font);

private:
	Ui::FixedFontSelector ui;
};

#endif
