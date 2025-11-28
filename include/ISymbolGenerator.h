/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef ISYMBOL_GENERATOR_H_20130808_
#define ISYMBOL_GENERATOR_H_20130808_

class QString;

class ISymbolGenerator {
public:
	virtual ~ISymbolGenerator() = default;

public:
	virtual bool generateSymbolFile(const QString &filename, const QString &symbol_file) = 0;
};

#endif
