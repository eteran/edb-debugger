/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DIALOG_ASSEMBLER_H_20130611_
#define DIALOG_ASSEMBLER_H_20130611_

#include "IRegion.h"
#include "Types.h"
#include "ui_DialogAssembler.h"
#include <QDialog>

namespace AssemblerPlugin {

class DialogAssembler : public QDialog {
	Q_OBJECT

public:
	explicit DialogAssembler(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogAssembler() override = default;

public Q_SLOTS:
	void on_buttonBox_accepted();

public:
	void setAddress(edb::address_t address);

public:
	void showEvent(QShowEvent *event) override;

private:
	Ui::DialogAssembler ui;
	edb::address_t address_ = 0;
	size_t instructionSize_ = 0;
};

}

#endif
