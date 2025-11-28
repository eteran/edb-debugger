/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef OPTIONS_PAGE_H_20090706_
#define OPTIONS_PAGE_H_20090706_

#include "ui_OptionsPage.h"
#include <QWidget>

namespace DumpStatePlugin {

class OptionsPage : public QWidget {
	Q_OBJECT

public:
	explicit OptionsPage(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~OptionsPage() override = default;

public:
	void showEvent(QShowEvent *event) override;

public Q_SLOTS:
	void on_instructionsBeforeIP_valueChanged(int i);
	void on_instructionsAfterIP_valueChanged(int i);
	void on_colorizeOutput_toggled(bool value);

private:
	Ui::OptionsPage ui;
};

}

#endif
