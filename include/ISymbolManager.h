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

#ifndef ISYMBOL_MANAGER_H_20110307_
#define ISYMBOL_MANAGER_H_20110307_

#include "Types.h"
#include <QHash>
#include <memory>
#include <vector>

class QString;
class Symbol;
class ISymbolGenerator;

class ISymbolManager {
public:
	virtual ~ISymbolManager() = default;

public:
	virtual const std::vector<std::shared_ptr<Symbol>> symbols() const                 = 0;
	virtual const std::shared_ptr<Symbol> find(const QString &name) const              = 0;
	virtual const std::shared_ptr<Symbol> find(edb::address_t address) const           = 0;
	virtual const std::shared_ptr<Symbol> findNearSymbol(edb::address_t address) const = 0;
	virtual void addSymbol(const std::shared_ptr<Symbol> &symbol)                      = 0;
	virtual void clear()                                                               = 0;
	virtual void loadSymbolFile(const QString &filename, edb::address_t base)          = 0;
	virtual void setSymbolGenerator(ISymbolGenerator *generator)                       = 0;
	virtual void setLabel(edb::address_t address, const QString &label)                = 0;
	virtual QString findAddressName(edb::address_t address, bool prefixed = true)      = 0;
	virtual QHash<edb::address_t, QString> labels() const                              = 0;
	virtual QStringList files() const                                                  = 0;
};

#endif
