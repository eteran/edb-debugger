/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "QLongValidator.h"

/**
 * @brief
 */
QLongValidator::QLongValidator(QObject *parent)
	: QValidator(parent) {
}

/**
 * @brief
 */
QLongValidator::QLongValidator(QLongValidator::value_type minimum, QLongValidator::value_type maximum, QObject *parent)
	: QValidator(parent), minimum_(minimum), maximum_(maximum) {
}

/**
 * @brief
 */
QLongValidator::value_type QLongValidator::bottom() const {
	return minimum_;
}

/**
 * @brief
 */
void QLongValidator::setBottom(QLongValidator::value_type bottom) {
	minimum_ = bottom;
}

/**
 * @brief
 */
void QLongValidator::setRange(QLongValidator::value_type bottom, QLongValidator::value_type top) {
	setBottom(bottom);
	setTop(top);
}

/**
 * @brief
 */
void QLongValidator::setTop(QLongValidator::value_type top) {
	maximum_ = top;
}

/**
 * @brief
 */
QLongValidator::value_type QLongValidator::top() const {
	return maximum_;
}

/**
 * @brief
 */
QValidator::State QLongValidator::validate(QString &input, int &pos) const {
	Q_UNUSED(pos)

	if (input.isEmpty()) {
		return QValidator::Acceptable;
	}

	if (input == "-") {
		return QValidator::Intermediate;
	}

	bool ok;
	const value_type temp = input.toLongLong(&ok);
	if (!ok) {
		return QValidator::Invalid;
	}

	return (temp >= bottom() && temp <= top()) ? QValidator::Acceptable : QValidator::Invalid;
}
