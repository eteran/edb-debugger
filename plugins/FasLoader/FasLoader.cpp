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
 * @brief FasLoader::FasLoader
 * @param parent
 */
FasLoader::FasLoader(QObject *parent)
	: QObject(parent) {
}

/**
 * @brief FasLoader::menu
 * @param parent
 * @return
 */
QMenu *FasLoader::menu(QWidget *parent) {

	Q_ASSERT(parent);

	if (!menu_) {
		menu_ = new QMenu(tr("FasLoader"), parent);
		menu_->addAction(tr("&Load *.fas symbols"), this, SLOT(load()));
	}

	return menu_;
}

/**
 * @brief FasLoader::load
 */
void FasLoader::load() {
	if (edb::v1::debugger_core) {
		if (IProcess *process = edb::v1::debugger_core->process()) {
			const QString fileName = process->executable();
			QString fasName        = fileName;
			fasName.append(".fas");

			Fas::Core fasCore;
			fasCore.load(fasName.toUtf8().constData());

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
