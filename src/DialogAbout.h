/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DIALOG_ABOUT_H_20150802_
#define DIALOG_ABOUT_H_20150802_

#include "ui_DialogAbout.h"
#include <QDialog>

class DialogAbout final : public QDialog {
	Q_OBJECT

public:
	explicit DialogAbout(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogAbout() override = default;

private:
	Ui::DialogAbout ui;
};

#endif
