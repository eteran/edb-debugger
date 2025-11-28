/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DIALOG_BINARY_STRING_H_20061101_
#define DIALOG_BINARY_STRING_H_20061101_

#include "ui_DialogBinaryString.h"
#include <QDialog>
#include <QPointer>

class QListWidgetItem;

namespace BinarySearcherPlugin {

class DialogResults;

class DialogBinaryString : public QDialog {
	Q_OBJECT

public:
	explicit DialogBinaryString(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogBinaryString() override = default;

private:
	void doFind();

private:
	Ui::DialogBinaryString ui;
	QPushButton *buttonFind_ = nullptr;
	QPointer<DialogResults> results_;
};

}

#endif
