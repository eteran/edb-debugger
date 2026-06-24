/*
 * Copyright (C) 2015 Ruslan Kabatsayev <b7.10110111@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "VolatileNameField.h"

namespace ODbgRegisterView {

/**
 * @brief Constructs a VolatileNameField with the specified field width, value formatter, parent widget, and window flags.
 *
 * @param fieldWidth The width of the field.
 * @param valueFormatter A function that returns the formatted value as a QString.
 * @param parent The parent widget (default is nullptr).
 * @param f The window flags (default is Qt::WindowFlags()).
 */
VolatileNameField::VolatileNameField(int fieldWidth, std::function<QString()> valueFormatter, QWidget *parent, Qt::WindowFlags f)
	: FieldWidget(fieldWidth, "", parent, f), valueFormatter(std::move(valueFormatter)) {
}

/**
 * @brief Returns the text representation of the volatile name field by invoking the value formatter function.
 *
 * @return The formatted text as a QString.
 */
QString VolatileNameField::text() const {
	return valueFormatter();
}

}
