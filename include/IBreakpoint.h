/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef IBREAKPOINT_H_20060720_
#define IBREAKPOINT_H_20060720_

#include "Types.h"

#include <QString>
#include <exception>

class QByteArray;

class BreakpointCreationError : public std::exception {
	[[nodiscard]] const char *what() const noexcept override {
		return "BreakpointCreationError";
	}
};

class IBreakpoint {
protected:
	IBreakpoint() = default;

public:
	virtual ~IBreakpoint() = default;

	enum class TypeId : int {
		Automatic, // should be the default if the user hasn't chosen anything other
	};

	struct BreakpointType {
		TypeId type;
		QString description;
	};

public:
	[[nodiscard]] virtual edb::address_t address() const       = 0;
	[[nodiscard]] virtual uint64_t hitCount() const            = 0;
	[[nodiscard]] virtual bool enabled() const                 = 0;
	[[nodiscard]] virtual bool oneTime() const                 = 0;
	[[nodiscard]] virtual bool internal() const                = 0;
	[[nodiscard]] virtual const uint8_t *originalBytes() const = 0;
	[[nodiscard]] virtual size_t size() const                  = 0;
	[[nodiscard]] virtual TypeId type() const                  = 0;

public:
	virtual bool enable()                = 0;
	virtual bool disable()               = 0;
	virtual void hit()                   = 0;
	virtual void setOneTime(bool value)  = 0;
	virtual void setInternal(bool value) = 0;
	virtual void setType(TypeId type)    = 0;

public:
	QString condition;
	quint64 tag = 0;
};

Q_DECLARE_METATYPE(IBreakpoint::TypeId)

#endif
