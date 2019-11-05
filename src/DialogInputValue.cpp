/*
Copyright (C) 2006 - 2015 Evan Teran
                          evan.teran@gmail.com

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "DialogInputValue.h"
#include "QLongValidator.h"
#include "QULongValidator.h"
#include "Register.h"
#include "edb.h"

#include <QDebug>
#include <QRegExpValidator>

#include <limits>

//------------------------------------------------------------------------------
// Name: DialogInputValue
// Desc:
//------------------------------------------------------------------------------
DialogInputValue::DialogInputValue(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {

	ui.setupUi(this);

	// Apply some defaults
	ui.hexInput->setValidator(new QRegExpValidator(QRegExp("[A-Fa-f0-9]{0,16}"), this));
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
	ui.signedInput->setText(QString("%1").arg(reg.valueAsSignedInteger()));
	ui.unsignedInput->setText(QString("%1").arg(reg.valueAsInteger()));

	const auto regex                = QString("[A-Fa-f0-9]{0,%1}").arg(reg.bitSize() / 4);
	const std::uint64_t unsignedMax = (reg.bitSize() == 64 ? -1 : (1ull << (reg.bitSize())) - 1); // Avoid UB
	const std::int64_t signedMin    = 1ull << (reg.bitSize() - 1);
	const std::int64_t signedMax    = unsignedMax >> 1;
	mask_                           = unsignedMax;
	valueLength_                    = reg.bitSize() / 8;

	ui.hexInput->setValidator(new QRegExpValidator(QRegExp(regex), this));
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
