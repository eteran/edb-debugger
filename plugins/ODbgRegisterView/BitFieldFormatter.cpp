/*
 * Copyright (C) 2015 Ruslan Kabatsayev <b7.10110111@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "BitFieldFormatter.h"
#include "BitFieldDescription.h"
#include "RegisterView.h"

namespace ODbgRegisterView {

BitFieldFormatter::BitFieldFormatter(const BitFieldDescription &bfd)
	: valueNames(bfd.valueNames) {
}

QString BitFieldFormatter::operator()(const QString &str) const {
	assert(str.length());
	if (str.isEmpty()) {
		return str; // for release builds have defined behavior
	}

	if (str[0] == '?') {
		return "????";
	}

	bool parseOK    = false;
	const int value = str.toInt(&parseOK);
	if (!parseOK) {
		return "????";
	}

	assert(0 <= value);
	assert(std::size_t(value) < valueNames.size());
	return valueNames[value];
}

}
