/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef IBINARY_H_20070718_
#define IBINARY_H_20070718_

#include "API.h"
#include "Types.h"
#include <memory>
#include <vector>

class IRegion;

class EDB_EXPORT IBinary {
public:
	struct Header {
		edb::address_t address;
		size_t size;
		// TODO(eteran): maybe label/type/etc...
	};

public:
	virtual ~IBinary() = default;

public:
	[[nodiscard]] virtual bool native() const                 = 0;
	[[nodiscard]] virtual edb::address_t entryPoint()         = 0;
	[[nodiscard]] virtual size_t headerSize() const           = 0;
	[[nodiscard]] virtual const void *header() const          = 0;
	[[nodiscard]] virtual std::vector<Header> headers() const = 0;

public:
	using create_func_ptr_t = std::unique_ptr<IBinary> (*)(const std::shared_ptr<IRegion> &);
};

#endif
