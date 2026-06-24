/*
 * Copyright (C) 2018 - 2025  Evan Teran <evan.teran@gmail.com>
 * Copyright (C) 2018 darkprof <dark_prof@mail.ru>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "FasLoader.hpp"
#include "IDebugger.h"
#include "IProcess.h"
#include "ISymbolManager.h"
#include "Symbol.h"
#include "edb.h"

#include <QMenu>
#include <QSettings>

namespace FasLoaderPlugin {

/**
 * @brief Constructs a FasLoader object with the specified parent QObject.
 *
 * @param parent The parent QObject for this plugin.
 */
FasLoader::FasLoader(QObject *parent)
	: QObject(parent) {
}

/**
 * @brief Returns the menu for the FasLoader plugin, creating it if it doesn't already exist.
 *
 * @param parent The parent widget for the menu.
 * @return A pointer to the created menu.
 */
QMenu *FasLoader::menu(QWidget *parent) {

	Q_ASSERT(parent);

	if (!menu_) {
		menu_ = new QMenu(tr("FasLoader"), parent);
		menu_->addAction(tr("&Load *.fas symbols"), this, &FasLoader::load);
	}

	return menu_;
}

/**
 * @brief Loads symbols from a *.fas file associated with the current process's executable and adds them to the symbol manager.
 */
void FasLoader::load() {
	if (edb::v1::debugger_core) {
		if (IProcess *process = edb::v1::debugger_core->process()) {
			const QString fileName = process->executable();
			QString fasName        = fileName;
			fasName.append(".fas");

			Fas::Core fasCore;
			fasCore.load(fasName.toStdString());

			auto pluginSymbols = fasCore.getSymbols();
			for (auto pluginSymbol : pluginSymbols) {

				auto symbol = std::make_shared<Symbol>();

				symbol->file    = fileName;
				symbol->address = pluginSymbol.value;
				symbol->name    = QString::fromStdString(pluginSymbol.name);
				symbol->size    = pluginSymbol.size;
				if (pluginSymbol.size > 0) {
					symbol->type = 'd';
				}

				edb::v1::symbol_manager().addSymbol(symbol);
			}
		}
	}
}

}
