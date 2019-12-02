/*
Copyright (C) 2006 - 2015 Evan Teran
                          evan.teran@gmail.com

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef IBREAKPOINT_H_20060720_
#define IBREAKPOINT_H_20060720_

#include "Types.h"

#include <QString>
#include <exception>

class QByteArray;

class BreakpointCreationError : public std::exception {
	const char *what() const noexcept override {
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
	virtual edb::address_t address() const       = 0;
	virtual uint64_t hitCount() const            = 0;
	virtual bool enabled() const                 = 0;
	virtual bool oneTime() const                 = 0;
	virtual bool internal() const                = 0;
	virtual const uint8_t *originalBytes() const = 0;
	virtual size_t size() const                  = 0;
	virtual TypeId type() const                  = 0;

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
