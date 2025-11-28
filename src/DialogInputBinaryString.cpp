/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DialogInputBinaryString.h"

//------------------------------------------------------------------------------
// Name: DialogInputBinaryString
// Desc: constructor
//------------------------------------------------------------------------------
DialogInputBinaryString::DialogInputBinaryString(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {

	ui.setupUi(this);
}

//------------------------------------------------------------------------------
// Name: binary_string
// Desc: returns the binary string we wrap around
//------------------------------------------------------------------------------
BinaryString *DialogInputBinaryString::binaryString() const {
	return ui.binaryString;
}
