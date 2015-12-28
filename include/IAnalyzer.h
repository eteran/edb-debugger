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

#ifndef IANALYZER_20080630_H_
#define IANALYZER_20080630_H_

#include "Function.h"
#include "IRegion.h"
#include "Types.h"

#include <QSet>
#include <QHash>

class IAnalyzer {
public:
	virtual ~IAnalyzer() {}

public:
	typedef QHash<edb::address_t, Function> FunctionMap;

public:
	enum AddressCategory {
		ADDRESS_FUNC_UNKNOWN = 0x00,
		ADDRESS_FUNC_START   = 0x01,
		ADDRESS_FUNC_BODY    = 0x02,
		ADDRESS_FUNC_END     = 0x04
	};

public:
	virtual AddressCategory category(edb::address_t address) const = 0;
	virtual FunctionMap functions(const IRegion::pointer &region) const = 0;
	virtual FunctionMap functions() const = 0;
	virtual QSet<edb::address_t> specified_functions() const { return QSet<edb::address_t>(); }
	virtual edb::address_t find_containing_function(edb::address_t address, bool *ok) const = 0;
	virtual void analyze(const IRegion::pointer &region) = 0;
	virtual void invalidate_analysis() = 0;
	virtual void invalidate_analysis(const IRegion::pointer &region) = 0;
};

#endif
