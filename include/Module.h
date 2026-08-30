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

inline bool operator==(const Module &lhs, const Module &rhs) {
	return lhs.name == rhs.name && lhs.baseAddress == rhs.baseAddress;
}

inline bool operator<(const Module &lhs, const Module &rhs) {
	return lhs.baseAddress < rhs.baseAddress || (lhs.baseAddress == rhs.baseAddress && lhs.name < rhs.name);
}

// NOTE(eteran): this type is different between Qt5 and Qt6, so we just use whatever qHash returns for a uint32_t.
using hash_type = decltype(qHash(0u, 0));

inline hash_type qHash(const Module &module, hash_type seed = 0) {
	return qHash(module.name, seed) ^ qHash(module.baseAddress.toUint(), seed);
}

#endif
