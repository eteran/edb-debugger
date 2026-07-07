/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MODULE_H_20191119_
#define MODULE_H_20191119_

#include "Types.h"
#include <QString>

struct Module {
	QString name;
	edb::address_t baseAddress;
};

bool operator==(const Module &lhs, const Module &rhs) {
	return lhs.name == rhs.name && lhs.baseAddress == rhs.baseAddress;
}

namespace std {
template <>
struct hash<Module> {
	std::size_t operator()(const Module &m) const noexcept {
		// Compute individual hashes using built-in specializations
		std::size_t h1 = std::hash<QString>{}(m.name);
		std::size_t h2 = std::hash<uint64_t>{}(m.baseAddress.toUint());

		// Combine the hashes (a robust shifting technique minimizes collisions)
		return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
	}
};
}

#endif
