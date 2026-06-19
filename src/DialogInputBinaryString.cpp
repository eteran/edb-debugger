/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DialogInputBinaryString.h"

/**
 * @brief Constructor for the DialogInputBinaryString class.
 * @param parent The parent widget.
 * @param f The window flags.
 */
DialogInputBinaryString::DialogInputBinaryString(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {

	ui.setupUi(this);
}

/**
 * @brief Returns the binary string we wrap around.
 * @return A pointer to the binary string.
 */
BinaryString *DialogInputBinaryString::binaryString() const {
	return ui.binaryString;
}
