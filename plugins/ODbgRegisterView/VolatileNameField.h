/*
 * Copyright (C) 2015 Ruslan Kabatsayev <b7.10110111@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef VOLATILE_NAME_FIELD_H_20151031_
#define VOLATILE_NAME_FIELD_H_20151031_

#include "FieldWidget.h"
#include <QString>
#include <functional>

namespace ODbgRegisterView {

class VolatileNameField : public FieldWidget {
	Q_OBJECT

private:
	std::function<QString()> valueFormatter;

public:
	VolatileNameField(int fieldWidth, std::function<QString()> valueFormatter, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	[[nodiscard]] QString text() const override;
};

}

#endif
