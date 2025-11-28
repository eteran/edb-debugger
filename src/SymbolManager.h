/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
	[[nodiscard]] QHash<edb::address_t, QString> labels() const override;
	[[nodiscard]] QString findAddressName(edb::address_t address, bool prefixed = true) override;
	[[nodiscard]] QStringList files() const override;
	[[nodiscard]] std::shared_ptr<Symbol> find(const QString &name) const override;
	[[nodiscard]] std::shared_ptr<Symbol> find(edb::address_t address) const override;
	[[nodiscard]] std::shared_ptr<Symbol> findNearSymbol(edb::address_t address) const override;
	[[nodiscard]] std::vector<std::shared_ptr<Symbol>> symbols() const override;
	void addSymbol(const std::shared_ptr<Symbol> &symbol) override;
	void clear() override;
	void loadSymbolFile(const QString &filename, edb::address_t base) override;
	void setLabel(edb::address_t address, const QString &label) override;
	void setSymbolGenerator(ISymbolGenerator *generator) override;

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
