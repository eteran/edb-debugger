/*
 * Copyright (C) 2015 Ruslan Kabatsayev <b7.10110111@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef OPTIONS_PAGE_H_20151113_
#define OPTIONS_PAGE_H_20151113_

#include "ui_OptionsPage.h"
#include <QWidget>
#include <memory>

namespace BinaryInfoPlugin {

class OptionsPage : public QWidget {
	Q_OBJECT

public:
	explicit OptionsPage(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~OptionsPage() override = default;

public:
	void showEvent(QShowEvent *event) override;

public Q_SLOTS:
	void on_checkBox_toggled(bool checked = false);
	void on_txtDebugDir_textChanged(const QString &text);
	void on_btnDebugDir_clicked();

private:
	Ui::OptionsPage ui;
};

}

#endif
