/*
Copyright (C) 2015 Ruslan Kabatsayev <b7.10110111@gmail.com>

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

#include "DialogEditFPU.h"
#include "EntryGridKeyUpDownEventFilter.h"
#include "Float80Edit.h"
#include "util/Float.h"

#include <QDebug>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QRegExp>
#include <QVBoxLayout>
#include <array>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>

namespace ODbgRegisterView {
namespace {

long double readFloat(const QString &strInput, bool &ok) {
	ok = false;
	const QString str(strInput.toLower().trimmed());

	if (const auto value = util::full_string_to_float<long double>(str.toStdString())) {
		ok = true;
		return *value;
	}

	// OK, so either it is invalid/unfinished, or it's some special value
	// We still do want the user to be able to enter common special values
	long double value;

	static const std::array<std::uint8_t, 16> positiveInf{0, 0, 0, 0, 0, 0, 0, 0x80, 0xff, 0x7f, 0, 0, 0, 0, 0, 0};
	static const std::array<std::uint8_t, 16> negativeInf{0, 0, 0, 0, 0, 0, 0, 0x80, 0xff, 0xff, 0, 0, 0, 0, 0, 0};
	static const std::array<std::uint8_t, 16> positiveSNaN{0, 0, 0, 0, 0, 0, 0, 0x90, 0xff, 0x7f, 0, 0, 0, 0, 0, 0};
	static const std::array<std::uint8_t, 16> negativeSNaN{0, 0, 0, 0, 0, 0, 0, 0x90, 0xff, 0xff, 0, 0, 0, 0, 0, 0};

	// Indefinite values are used for QNaN
	static const std::array<std::uint8_t, 16> positiveQNaN{0, 0, 0, 0, 0, 0, 0, 0xc0, 0xff, 0x7f, 0, 0, 0, 0, 0, 0};
	static const std::array<std::uint8_t, 16> negativeQNaN{0, 0, 0, 0, 0, 0, 0, 0xc0, 0xff, 0xff, 0, 0, 0, 0, 0, 0};

	if (str == "+snan" || str == "snan")
		std::memcpy(&value, &positiveSNaN, sizeof(value));
	else if (str == "-snan")
		std::memcpy(&value, &negativeSNaN, sizeof(value));
	else if (str == "+qnan" || str == "qnan" || str == "nan")
		std::memcpy(&value, &positiveQNaN, sizeof(value));
	else if (str == "-qnan")
		std::memcpy(&value, &negativeQNaN, sizeof(value));
	else if (str == "+inf" || str == "inf")
		std::memcpy(&value, &positiveInf, sizeof(value));
	else if (str == "-inf")
		std::memcpy(&value, &negativeInf, sizeof(value));
	else
		return 0;

	ok = true;
	return value;
}
}

DialogEditFPU::DialogEditFPU(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f), floatEntry_(new ODbgRegisterView::Float80Edit(this)), hexEntry_(new QLineEdit(this)) {

	setWindowTitle(tr("Modify Register"));
	setModal(true);
	const auto allContentsGrid = new QGridLayout();

	allContentsGrid->addWidget(new QLabel(tr("Float"), this), 0, 0);
	allContentsGrid->addWidget(new QLabel(tr("Hex"), this), 1, 0);

	allContentsGrid->addWidget(floatEntry_, 0, 1);
	allContentsGrid->addWidget(hexEntry_, 1, 1);

	connect(floatEntry_, &Float80Edit::textEdited, this, &DialogEditFPU::onFloatEdited);
	connect(hexEntry_, &QLineEdit::textEdited, this, &DialogEditFPU::onHexEdited);

	hexEntry_->setValidator(new QRegExpValidator(QRegExp("[0-9a-fA-F ]{,20}"), this));
	connect(floatEntry_, &Float80Edit::defocussed, this, &DialogEditFPU::updateFloatEntry);

	hexEntry_->installEventFilter(this);
	floatEntry_->installEventFilter(this);

	const auto okCancel = new QDialogButtonBox(this);
	okCancel->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);

	connect(okCancel, &QDialogButtonBox::accepted, this, &DialogEditFPU::accept);
	connect(okCancel, &QDialogButtonBox::rejected, this, &DialogEditFPU::reject);

	const auto dialogLayout = new QVBoxLayout(this);
	dialogLayout->addLayout(allContentsGrid);
	dialogLayout->addWidget(okCancel);

	setTabOrder(floatEntry_, hexEntry_);
	setTabOrder(hexEntry_, okCancel);
}

void DialogEditFPU::updateFloatEntry() {
	floatEntry_->setValue(value_);
}

void DialogEditFPU::updateHexEntry() {
	hexEntry_->setText(value_.toHexString());
}

bool DialogEditFPU::eventFilter(QObject *obj, QEvent *event) {
	return entry_grid_key_event_filter(this, obj, event);
}

void DialogEditFPU::setValue(const Register &newReg) {
	reg_   = newReg;
	value_ = reg_.value<edb::value80>();
	updateFloatEntry();
	updateHexEntry();
	setWindowTitle(tr("Modify %1").arg(reg_.name().toUpper()));
	floatEntry_->setFocus(Qt::OtherFocusReason);
}

Register DialogEditFPU::value() const {
	Register ret(reg_);
	ret.setValueFrom(value_);
	return ret;
}

void DialogEditFPU::onHexEdited(const QString &input) {
	QString readable(input.trimmed());
	readable.replace(' ', "");

	while (readable.size() < 20) {
		readable = '0' + readable;
	}

	const auto byteArray = QByteArray::fromHex(readable.toLatin1());
	auto source          = byteArray.constData();
	auto dest            = reinterpret_cast<unsigned char *>(&value_);

	for (std::size_t i = 0; i < sizeof(value_); ++i) {
		dest[i] = source[sizeof(value_) - i - 1];
	}

	updateFloatEntry();
}

void DialogEditFPU::onFloatEdited(const QString &str) {
	bool ok;
	const long double value = readFloat(str, ok);

	if (ok) {
		value_ = edb::value80(value);
	}

	updateHexEntry();
}

}
