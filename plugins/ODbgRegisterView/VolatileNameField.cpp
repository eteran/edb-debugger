/*
 * Copyright (C) 2015 Ruslan Kabatsayev <b7.10110111@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "VolatileNameField.h"

namespace ODbgRegisterView {

VolatileNameField::VolatileNameField(int fieldWidth, std::function<QString()> valueFormatter, QWidget *parent, Qt::WindowFlags f)
	: FieldWidget(fieldWidth, "", parent, f), valueFormatter(std::move(valueFormatter)) {
}

QString VolatileNameField::text() const {
	return valueFormatter();
}

}
