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

#include "Float80Edit.h"
#include "FloatX.h"
#include <QApplication>

namespace ODbgRegisterView {

Float80Edit::Float80Edit(QWidget *parent)
	: QLineEdit(parent) {

	setValidator(new FloatXValidator<long double>(this));
}

void Float80Edit::setValue(edb::value80 input) {
	setText(format_float(input));
}

QSize Float80Edit::sizeHint() const {
	const auto baseHint = QLineEdit::sizeHint();
	// Default size hint gives space for about 15-20 chars. We need about 30.
	return QSize(baseHint.width() * 2, baseHint.height()).expandedTo(QApplication::globalStrut());
}

void Float80Edit::focusOutEvent(QFocusEvent *e) {
	QLineEdit::focusOutEvent(e);
	Q_EMIT defocussed();
}

}
