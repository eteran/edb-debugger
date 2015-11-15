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

#ifndef ISYMBOL_MANAGER_20110307_H_
#define ISYMBOL_MANAGER_20110307_H_

#include "API.h"
#include "Types.h"
#include "Symbol.h"
#include <QList>
#include <QHash>

class QString;
class ISymbolGenerator;

class EDB_EXPORT ISymbolManager {
public:
	virtual ~ISymbolManager() {}

public:
	virtual const QList<Symbol::pointer> symbols() const = 0;
	virtual const Symbol::pointer find(const QString &name) const = 0;
	virtual const Symbol::pointer find(edb::address_t address) const = 0;
	virtual const Symbol::pointer find_near_symbol(edb::address_t address) const = 0;
	virtual void add_symbol(const Symbol::pointer &symbol) = 0;
	virtual void clear() = 0;
	virtual void load_symbol_file(const QString &filename, edb::address_t base) = 0;
	virtual void set_symbol_generator(ISymbolGenerator *generator) = 0;
	virtual void set_label(edb::address_t address, const QString &label) = 0;
	virtual QString find_address_name(edb::address_t address) = 0;
	virtual QHash<edb::address_t, QString> labels() const = 0;
	virtual QList<QString> files() const = 0;
};

#endif
