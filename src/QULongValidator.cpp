/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "QULongValidator.h"

//------------------------------------------------------------------------------
// Name: QULongValidator
// Desc:
//------------------------------------------------------------------------------
QULongValidator::QULongValidator(QObject *parent)
	: QValidator(parent) {
}

//------------------------------------------------------------------------------
// Name: QULongValidator
// Desc:
//------------------------------------------------------------------------------
QULongValidator::QULongValidator(QULongValidator::value_type minimum, QULongValidator::value_type maximum, QObject *parent)
	: QValidator(parent), minimum_(minimum), maximum_(maximum) {
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
	Q_UNUSED(pos)

	if (input.isEmpty()) {
		return QValidator::Acceptable;
	}

	bool ok;
	const value_type temp = input.toULongLong(&ok);
	if (!ok) {
		return QValidator::Invalid;
	}

	return (temp >= bottom() && temp <= top()) ? QValidator::Acceptable : QValidator::Invalid;
}
