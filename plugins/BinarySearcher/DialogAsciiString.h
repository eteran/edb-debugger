/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DIALOG_ASCII_STRING_H_20082201_
#define DIALOG_ASCII_STRING_H_20082201_

#include "ui_DialogAsciiString.h"
#include <QDialog>

class QListWidgetItem;

namespace BinarySearcherPlugin {

class DialogAsciiString : public QDialog {
	Q_OBJECT

public:
	explicit DialogAsciiString(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogAsciiString() override = default;

protected:
	void showEvent(QShowEvent *event) override;

private:
	void doFind();

private:
	Ui::DialogAsciiString ui;
	QPushButton *buttonFind_ = nullptr;
};

}

#endif
