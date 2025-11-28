/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
	[[nodiscard]] virtual QHash<edb::address_t, QString> labels() const                         = 0;
	[[nodiscard]] virtual QString findAddressName(edb::address_t address, bool prefixed = true) = 0;
	[[nodiscard]] virtual QStringList files() const                                             = 0;
	[[nodiscard]] virtual std::shared_ptr<Symbol> find(const QString &name) const               = 0;
	[[nodiscard]] virtual std::shared_ptr<Symbol> find(edb::address_t address) const            = 0;
	[[nodiscard]] virtual std::shared_ptr<Symbol> findNearSymbol(edb::address_t address) const  = 0;
	[[nodiscard]] virtual std::vector<std::shared_ptr<Symbol>> symbols() const                  = 0;
	virtual void addSymbol(const std::shared_ptr<Symbol> &symbol)                               = 0;
	virtual void clear()                                                                        = 0;
	virtual void loadSymbolFile(const QString &filename, edb::address_t base)                   = 0;
	virtual void setLabel(edb::address_t address, const QString &label)                         = 0;
	virtual void setSymbolGenerator(ISymbolGenerator *generator)                                = 0;
};

#endif
