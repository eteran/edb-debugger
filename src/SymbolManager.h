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

#ifndef SYMBOLMANAGER_20060814_H_
#define SYMBOLMANAGER_20060814_H_

#include "ISymbolManager.h"
#include <QHash>
#include <QMap>
#include <QSet>
#include <QString>

class SymbolManager : public ISymbolManager {
public:
	SymbolManager();

public:
	virtual const QList<std::shared_ptr<Symbol>> symbols() const override;
	virtual const std::shared_ptr<Symbol> find(const QString &name) const override;
	virtual const std::shared_ptr<Symbol> find(edb::address_t address) const override;
	virtual const std::shared_ptr<Symbol> find_near_symbol(edb::address_t address) const override;
	virtual void add_symbol(const std::shared_ptr<Symbol> &symbol) override;
	virtual void clear() override;
	virtual void load_symbol_file(const QString &filename, edb::address_t base) override;
	virtual void set_symbol_generator(ISymbolGenerator *generator) override;
	virtual void set_label(edb::address_t address, const QString &label) override;
	virtual QString find_address_name(edb::address_t address,bool prefixed=true) override;
	virtual QHash<edb::address_t, QString> labels() const override;
	virtual QList<QString> files() const override;

private:
	bool process_symbol_file(const QString &f, edb::address_t base, const QString &library_filename, bool allow_retry);

private:
	QSet<QString>                          symbol_files_;
	QList<std::shared_ptr<Symbol>>                 symbols_;
	QMap<edb::address_t, std::shared_ptr<Symbol>>  symbols_by_address_;
	QHash<QString, QList<std::shared_ptr<Symbol>>> symbols_by_file_;
	QHash<QString, std::shared_ptr<Symbol>>        symbols_by_name_;
	ISymbolGenerator                      *symbol_generator_;
	bool                                   show_path_notice_;
	QHash<edb::address_t, QString>         labels_;
	QHash<QString, edb::address_t>         labels_by_name_;

};

#endif

