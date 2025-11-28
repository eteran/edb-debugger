/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef BIT_FIELD_DESCRIPTION_H_20191119_
#define BIT_FIELD_DESCRIPTION_H_20191119_

#include <functional>
#include <vector>

class QString;

namespace ODbgRegisterView {

struct BitFieldDescription {
	int textWidth;
	std::vector<QString> valueNames;
	std::vector<QString> setValueTexts;
	std::function<bool(unsigned, unsigned)> const valueEqualComparator;

	BitFieldDescription(
		int textWidth,
		std::vector<QString> valueNames,
		std::vector<QString> setValueTexts,
		std::function<bool(unsigned, unsigned)> valueEqualComparator = [](unsigned a, unsigned b) { return a == b; });
};

}

#endif
