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

uint qHash(const Module &module, uint seed = 0) {
	return qHash(module.name, seed) ^ qHash(module.baseAddress.toUint(), seed);
}

#endif
