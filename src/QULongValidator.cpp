/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "QULongValidator.h"

/**
 * @brief
 */
QULongValidator::QULongValidator(QObject *parent)
	: QValidator(parent) {
}

/**
 * @brief
 */
QULongValidator::QULongValidator(QULongValidator::value_type minimum, QULongValidator::value_type maximum, QObject *parent)
	: QValidator(parent), minimum_(minimum), maximum_(maximum) {
}

/**
 * @brief
 */
QULongValidator::value_type QULongValidator::bottom() const {
	return minimum_;
}

/**
 * @brief Sets the minimum value for the validator.
 * @param bottom The new minimum value.
 */
void QULongValidator::setBottom(QULongValidator::value_type bottom) {
	minimum_ = bottom;
}

/**
 * @brief
 */
void QULongValidator::setRange(QULongValidator::value_type bottom, QULongValidator::value_type top) {
	setBottom(bottom);
	setTop(top);
}

/**
 * @brief
 */
void QULongValidator::setTop(QULongValidator::value_type top) {
	maximum_ = top;
}

/**
 * @brief
 */
QULongValidator::value_type QULongValidator::top() const {
	return maximum_;
}

/**
 * @brief
 */
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
