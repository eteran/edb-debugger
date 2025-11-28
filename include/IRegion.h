/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef IREGION_H_20120709_
#define IREGION_H_20120709_

#include "Types.h"
#include <memory>

class QString;

class IRegion {
public:
	using permissions_t = quint32;

public:
	virtual ~IRegion() = default;

public:
	[[nodiscard]] virtual IRegion *clone() const = 0;

public:
	[[nodiscard]] virtual bool accessible() const = 0;
	[[nodiscard]] virtual bool readable() const   = 0;
	[[nodiscard]] virtual bool writable() const   = 0;
	[[nodiscard]] virtual bool executable() const = 0;
	[[nodiscard]] virtual size_t size() const     = 0;

public:
	virtual void setPermissions(bool read, bool write, bool execute) = 0;
	virtual void setStart(edb::address_t address)                    = 0;
	virtual void setEnd(edb::address_t address)                      = 0;

public:
	[[nodiscard]] virtual edb::address_t start() const      = 0;
	[[nodiscard]] virtual edb::address_t end() const        = 0; // NOTE: is the address of one past the last byte of the region
	[[nodiscard]] virtual edb::address_t base() const       = 0;
	[[nodiscard]] virtual QString name() const              = 0;
	[[nodiscard]] virtual permissions_t permissions() const = 0;

public:
	[[nodiscard]] bool contains(edb::address_t address) const {
		return address >= start() && address < end();
	}

	template <class Pointer>
	[[nodiscard]] bool equals(const Pointer &other) const {

		if (!other) {
			return false;
		}

		return start() == other->start() &&
			   end() == other->end() &&
			   base() == other->base() &&
			   name() == other->name() &&
			   permissions() == other->permissions();
	}
};

#endif
