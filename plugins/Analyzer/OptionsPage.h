/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef OPTIONS_PAGE_H_20090706_
#define OPTIONS_PAGE_H_20090706_

#include "ui_OptionsPage.h"
#include <QWidget>

namespace AnalyzerPlugin {

class OptionsPage : public QWidget {
	Q_OBJECT

public:
	explicit OptionsPage(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~OptionsPage() override = default;

public:
	void showEvent(QShowEvent *event) override;

private:
	void checkBoxToggled(bool checked = false);

private:
	Ui::OptionsPage ui;
};

}

#endif
