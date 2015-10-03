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
#include "QULongValidator.h"
#include "QLongValidator.h"
#include "edb.h"

#include <QRegExpValidator>
#include <QDebug>

#include "ui_DialogInputValue.h"

//------------------------------------------------------------------------------
// Name: DialogInputValue
// Desc:
//------------------------------------------------------------------------------
DialogInputValue::DialogInputValue(QWidget *parent) : QDialog(parent), ui(new Ui::DialogInputValue), mask(-1ll), valueLength(sizeof(std::uint64_t)) {
	ui->setupUi(this);

	// Apply some defaults
	const QString regex = QString("[A-Fa-f0-9]{0,%1}").arg(16);
	ui->hexInput->setValidator(new QRegExpValidator(QRegExp(regex), this));
	ui->signedInput->setValidator(new QLongValidator(LONG_LONG_MIN, LONG_LONG_MAX, this));
	ui->unsignedInput->setValidator(new QULongValidator(0, ULONG_LONG_MAX, this));
}

//------------------------------------------------------------------------------
// Name: ~DialogInputValue
// Desc:
//------------------------------------------------------------------------------
DialogInputValue::~DialogInputValue() {
	delete ui;
}

//------------------------------------------------------------------------------
// Name: value
// Desc:
//------------------------------------------------------------------------------
edb::reg_t DialogInputValue::value() const {
	bool ok;
	return mask & edb::reg_t::fromHexString(ui->hexInput->text(),&ok);
}

//------------------------------------------------------------------------------
// Name: set_value
// Desc:
//------------------------------------------------------------------------------
void DialogInputValue::set_value(Register &reg) {
	if(reg.bitSize()>sizeof(edb::reg_t)*8) {
		qWarning() << "Warning: DialogInputValue::set_value(tooLargeRegister): such large registers are not supported yet";
		return;
	}
	ui->hexInput->setText(reg.toHexString());
	ui->signedInput->setText(QString("%1").arg(reg.valueAsSignedInteger()));
	ui->unsignedInput->setText(QString("%1").arg(reg.valueAsInteger()));

	const QString regex = QString("[A-Fa-f0-9]{0,%1}").arg(reg.bitSize()/4);
	const std::uint64_t unsignedMax=(reg.bitSize()==64 ? -1 : (1ull<<(reg.bitSize()))-1); // Avoid UB
	const std::int64_t signedMin=1ull<<(reg.bitSize()-1);
	const std::int64_t signedMax=unsignedMax>>1;
	mask=unsignedMax;
	valueLength=reg.bitSize()/8;

	ui->hexInput->setValidator(new QRegExpValidator(QRegExp(regex), this));
	ui->signedInput->setValidator(new QLongValidator(signedMin, signedMax, this));
	ui->unsignedInput->setValidator(new QULongValidator(0, unsignedMax, this));
}

//------------------------------------------------------------------------------
// Name: on_hexInput_textEdited
// Desc:
//------------------------------------------------------------------------------
void DialogInputValue::on_hexInput_textEdited(const QString &s) {
	bool ok;
	edb::reg_t value = edb::reg_t::fromHexString(s,&ok);

	if(!ok) {
		value = 0;
	}

	ui->signedInput->setText(value.signExtended(valueLength).signedToString());
	ui->unsignedInput->setText(value.unsignedToString());

}

//------------------------------------------------------------------------------
// Name: on_signedInput_textEdited
// Desc:
//------------------------------------------------------------------------------
void DialogInputValue::on_signedInput_textEdited(const QString &s) {
	bool ok;
	edb::reg_t value = edb::reg_t::fromSignedString(s,&ok);

	if(!ok) {
		value = 0;
	}

	ui->hexInput->setText(value.toHexString());
	ui->unsignedInput->setText(value.unsignedToString());
}

//------------------------------------------------------------------------------
// Name: on_unsignedInput_textEdited
// Desc:
//------------------------------------------------------------------------------
void DialogInputValue::on_unsignedInput_textEdited(const QString &s) {
	bool ok;
	edb::reg_t value = edb::reg_t::fromString(s,&ok);

	if(!ok) {
		value = 0;
	}
	ui->hexInput->setText(value.toHexString());
	ui->signedInput->setText(value.signedToString());
}
