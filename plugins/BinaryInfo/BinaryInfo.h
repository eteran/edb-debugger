/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef BINARY_INFO_H_20061122_
#define BINARY_INFO_H_20061122_

#include "IPlugin.h"
#include "ISymbolGenerator.h"
#include "Types.h"

class QMenu;

namespace BinaryInfoPlugin {

class BinaryInfo : public QObject, public IPlugin, public ISymbolGenerator {
	Q_OBJECT
	Q_INTERFACES(IPlugin)
	Q_PLUGIN_METADATA(IID "edb.IPlugin/1.0")
	Q_CLASSINFO("author", "Evan Teran")
	Q_CLASSINFO("url", "http://www.codef00.com")

public:
	explicit BinaryInfo(QObject *parent = nullptr);

private:
	void privateInit() override;
	[[nodiscard]] QWidget *optionsPage() override;

public:
	[[nodiscard]] QMenu *menu(QWidget *parent = nullptr) override;
	[[nodiscard]] QString extraArguments() const override;
	ArgumentStatus parseArguments(QStringList &args) override;

public:
	bool generateSymbolFile(const QString &filename, const QString &symbol_file) override;

public Q_SLOTS:
	void exploreHeader();

private:
	QMenu *menu_ = nullptr;
};

}

#endif
