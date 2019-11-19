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

#ifndef SYMBOL_MANAGER_H_20060814_
#define SYMBOL_MANAGER_H_20060814_

#include "ISymbolManager.h"

#include <QCoreApplication>
#include <QHash>
#include <QMap>
#include <QSet>

class QString;

class SymbolManager final : public ISymbolManager {
	Q_DECLARE_TR_FUNCTIONS(SymbolManager)

public:
	SymbolManager() = default;

public:
	const std::vector<std::shared_ptr<Symbol>> symbols() const override;
	const std::shared_ptr<Symbol> find(const QString &name) const override;
	const std::shared_ptr<Symbol> find(edb::address_t address) const override;
	const std::shared_ptr<Symbol> findNearSymbol(edb::address_t address) const override;
	void addSymbol(const std::shared_ptr<Symbol> &symbol) override;
	void clear() override;
	void loadSymbolFile(const QString &filename, edb::address_t base) override;
	void setSymbolGenerator(ISymbolGenerator *generator) override;
	void setLabel(edb::address_t address, const QString &label) override;
	QString findAddressName(edb::address_t address, bool prefixed = true) override;
	QHash<edb::address_t, QString> labels() const override;
	QStringList files() const override;

private:
	bool processSymbolFile(const QString &f, edb::address_t base, const QString &library_filename, bool allow_retry);

private:
	QSet<QString> symbolFiles_;
	std::vector<std::shared_ptr<Symbol>> symbols_;
	QMap<edb::address_t, std::shared_ptr<Symbol>> symbolsByAddress_;
	QHash<QString, QList<std::shared_ptr<Symbol>>> symbolsByFile_;
	QHash<QString, std::shared_ptr<Symbol>> symbolsByName_;
	QHash<edb::address_t, QString> labels_;
	QHash<QString, edb::address_t> labelsByName_;
	ISymbolGenerator *symbolGenerator_ = nullptr;
	bool showPathNotice_               = true;
};

#endif
