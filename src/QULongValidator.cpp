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

#include "QULongValidator.h"

//------------------------------------------------------------------------------
// Name: QULongValidator
// Desc:
//------------------------------------------------------------------------------
QULongValidator::QULongValidator(QObject *parent) : QValidator(parent), minimum_(0), maximum_(0) {
}

//------------------------------------------------------------------------------
// Name: QULongValidator
// Desc:
//------------------------------------------------------------------------------
QULongValidator::QULongValidator(QULongValidator::value_type minimum, QULongValidator::value_type maximum, QObject *parent) : QValidator(parent), minimum_(minimum), maximum_(maximum) {
}

//------------------------------------------------------------------------------
// Name: bottom
// Desc:
//------------------------------------------------------------------------------
QULongValidator::value_type QULongValidator::bottom() const {
	return minimum_;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
void QULongValidator::setBottom(QULongValidator::value_type bottom) {
	minimum_ = bottom;
}

//------------------------------------------------------------------------------
// Name: setRange
// Desc:
//------------------------------------------------------------------------------
void QULongValidator::setRange(QULongValidator::value_type bottom, QULongValidator::value_type top) {
	setBottom(bottom);
	setTop(top);
}

//------------------------------------------------------------------------------
// Name: setTop
// Desc:
//------------------------------------------------------------------------------
void QULongValidator::setTop(QULongValidator::value_type top) {
	maximum_ = top;
}

//------------------------------------------------------------------------------
// Name: top
// Desc:
//------------------------------------------------------------------------------
QULongValidator::value_type QULongValidator::top() const {
	return maximum_;
}

//------------------------------------------------------------------------------
// Name: validate
// Desc:
//------------------------------------------------------------------------------
QValidator::State QULongValidator::validate(QString &input, int &pos) const {
	Q_UNUSED(pos);

	if(input.isEmpty()) {
		return QValidator::Acceptable;
	}

	bool ok;
	const value_type temp = input.toULongLong(&ok);
	if(!ok) {
		return QValidator::Invalid;
	}

	return (temp >= bottom() && temp <= top()) ? QValidator::Acceptable : QValidator::Invalid;
}
