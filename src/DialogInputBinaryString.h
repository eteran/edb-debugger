/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DIALOG_INPUT_BINARY_STRING_H_20061127_
#define DIALOG_INPUT_BINARY_STRING_H_20061127_

#include <QDialog>

#include "ui_DialogInputBinaryString.h"

class BinaryString;

class DialogInputBinaryString : public QDialog {
	Q_OBJECT

public:
	explicit DialogInputBinaryString(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogInputBinaryString() override = default;

public:
	[[nodiscard]] BinaryString *binaryString() const;

private:
	Ui::DialogInputBinaryString ui;
};

#endif
