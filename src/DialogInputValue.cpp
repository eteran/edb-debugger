/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DialogInputValue.h"
#include "QLongValidator.h"
#include "QULongValidator.h"
#include "Register.h"
#include "edb.h"

#include <QDebug>
#include <QRegularExpressionValidator>

#include <limits>

//------------------------------------------------------------------------------
// Name: DialogInputValue
// Desc:
//------------------------------------------------------------------------------
DialogInputValue::DialogInputValue(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {

	ui.setupUi(this);

	// Apply some defaults
	ui.hexInput->setValidator(new QRegularExpressionValidator(QRegularExpression("[A-Fa-f0-9]{0,16}"), this));
	ui.signedInput->setValidator(new QLongValidator(std::numeric_limits<long long>::min(), std::numeric_limits<long long>::max(), this));
	ui.unsignedInput->setValidator(new QULongValidator(0, std::numeric_limits<unsigned long long>::max(), this));
}

//------------------------------------------------------------------------------
// Name: value
// Desc:
//------------------------------------------------------------------------------
edb::reg_t DialogInputValue::value() const {
	bool ok;
	return mask_ & edb::reg_t::fromHexString(ui.hexInput->text(), &ok);
}

//------------------------------------------------------------------------------
// Name: setValue
// Desc:
//------------------------------------------------------------------------------
void DialogInputValue::setValue(Register &reg) {
	if (reg.bitSize() > sizeof(edb::reg_t) * 8) {
		qWarning() << "Warning: DialogInputValue::setValue(tooLargeRegister): such large registers are not supported yet";
		return;
	}

	ui.hexInput->setText(reg.toHexString());
	ui.signedInput->setText(QStringLiteral("%1").arg(reg.valueAsSignedInteger()));
	ui.unsignedInput->setText(QStringLiteral("%1").arg(reg.valueAsInteger()));

	const auto regex                = QStringLiteral("[A-Fa-f0-9]{0,%1}").arg(reg.bitSize() / 4);
	const std::uint64_t unsignedMax = (reg.bitSize() == 64 ? -1 : (1ull << (reg.bitSize())) - 1); // Avoid UB
	const std::int64_t signedMin    = 1ull << (reg.bitSize() - 1);
	const std::int64_t signedMax    = unsignedMax >> 1;
	mask_                           = unsignedMax;
	valueLength_                    = reg.bitSize() / 8;

	ui.hexInput->setValidator(new QRegularExpressionValidator(QRegularExpression(regex), this));
	ui.signedInput->setValidator(new QLongValidator(signedMin, signedMax, this));
	ui.unsignedInput->setValidator(new QULongValidator(0, unsignedMax, this));
}

//------------------------------------------------------------------------------
// Name: on_hexInput_textEdited
// Desc:
//------------------------------------------------------------------------------
void DialogInputValue::on_hexInput_textEdited(const QString &s) {
	bool ok;
	auto value = edb::reg_t::fromHexString(s, &ok);

	if (!ok) {
		value = 0;
	}

	ui.signedInput->setText(value.signExtended(valueLength_).signedToString());
	ui.unsignedInput->setText(value.unsignedToString());
}

//------------------------------------------------------------------------------
// Name: on_signedInput_textEdited
// Desc:
//------------------------------------------------------------------------------
void DialogInputValue::on_signedInput_textEdited(const QString &s) {
	bool ok;
	auto value = edb::reg_t::fromSignedString(s, &ok);

	if (!ok) {
		value = 0;
	}

	ui.hexInput->setText(value.toHexString());
	ui.unsignedInput->setText(value.unsignedToString());
}

//------------------------------------------------------------------------------
// Name: on_unsignedInput_textEdited
// Desc:
//------------------------------------------------------------------------------
void DialogInputValue::on_unsignedInput_textEdited(const QString &s) {
	bool ok;
	auto value = edb::reg_t::fromString(s, &ok);

	if (!ok) {
		value = 0;
	}

	ui.hexInput->setText(value.toHexString());
	ui.signedInput->setText(value.signedToString());
}
