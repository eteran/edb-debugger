/*
Copyright (C) 2006 - 2015 * Evan Teran evan.teran@gmail.com
                          * darkprof dark_prof@mail.ru

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
