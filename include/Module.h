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

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
using hash_type = size_t;
#else
using hash_type = uint;
#endif

inline hash_type qHash(const Module &module, hash_type seed = 0) {
	return qHash(module.name, seed) ^ qHash(module.baseAddress.toUint(), seed);
}

#endif
