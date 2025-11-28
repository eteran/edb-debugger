/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SYMBOL_H_20110401_
#define SYMBOL_H_20110401_

#include "API.h"
#include "Types.h"
#include <QString>
#include <cstdint>

class Symbol {
public:
	QString file;
	QString name;
	QString name_no_prefix;
	edb::address_t address;
	uint32_t size;
	char type;

	[[nodiscard]] bool isCode() const { return type == 't' || type == 'T' || type == 'P'; }
	[[nodiscard]] bool isData() const { return !isCode(); }
	[[nodiscard]] bool isWeak() const { return type == 'W'; }
};

#endif
