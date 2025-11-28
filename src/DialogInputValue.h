/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DIALOG_INPUT_VALUE_H_20061101_
#define DIALOG_INPUT_VALUE_H_20061101_

#include "Types.h"

#include <QDialog>

#include "ui_DialogInputValue.h"

class Register;

class DialogInputValue : public QDialog {
	Q_OBJECT

public:
	explicit DialogInputValue(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogInputValue() override = default;

public Q_SLOTS:
	void on_hexInput_textEdited(const QString &);
	void on_signedInput_textEdited(const QString &);
	void on_unsignedInput_textEdited(const QString &);

public:
	[[nodiscard]] edb::reg_t value() const;
	void setValue(Register &reg);

private:
	Ui::DialogInputValue ui;
	edb::reg_t mask_         = -1ll;
	std::size_t valueLength_ = sizeof(std::uint64_t);
};

#endif
