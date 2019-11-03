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

#include "BinaryInfo.h"
#include "DialogRegions.h"
#include "ELFXX.h"
#include "IBinary.h"
#include "ISymbolManager.h"
#include "OptionsPage.h"
#include "PE32.h"
#include "edb.h"
#include "symbols.h"

#include <QDebug>
#include <QMenu>

#include <fstream>
#include <memory>

namespace BinaryInfoPlugin {

/**
 * @brief BinaryInfo::BinaryInfo
 * @param parent
 */
BinaryInfo::BinaryInfo(QObject *parent)
	: QObject(parent) {
}

/**
 * @brief BinaryInfo::privateInit
 */
void BinaryInfo::privateInit() {

	edb::v1::register_binary_info([](const std::shared_ptr<IRegion> &region) {
		return std::unique_ptr<IBinary>(new ELF32(region));
	});

	edb::v1::register_binary_info([](const std::shared_ptr<IRegion> &region) {
		return std::unique_ptr<IBinary>(new ELF64(region));
	});

	edb::v1::register_binary_info([](const std::shared_ptr<IRegion> &region) {
		return std::unique_ptr<IBinary>(new PE32(region));
	});

	edb::v1::symbol_manager().setSymbolGenerator(this);
}

/**
 * @brief BinaryInfo::optionsPage
 * @return
 */
QWidget *BinaryInfo::optionsPage() {
	return new OptionsPage;
}

/**
 * @brief BinaryInfo::menu
 * @param parent
 * @return
 */
QMenu *BinaryInfo::menu(QWidget *parent) {

	Q_ASSERT(parent);

	if (!menu_) {
		menu_ = new QMenu(tr("Binary Info"), parent);
		menu_->addAction(tr("&Explore Binary Header"), this, SLOT(exploreHeader()));
	}

	return menu_;
}

/**
 * @brief BinaryInfo::exploreHeader
 */
void BinaryInfo::exploreHeader() {
	static auto dialog = new DialogRegions(edb::v1::debugger_ui);
	dialog->show();
}

/**
 * @brief BinaryInfo::extraArguments
 * @return
 */
QString BinaryInfo::extraArguments() const {
	return " --symbols <filename>      : generate symbols for <filename> and exit";
}

/**
 * @brief BinaryInfo::parseArguments
 * @param args
 * @return
 */
IPlugin::ArgumentStatus BinaryInfo::parseArguments(QStringList &args) {

	if (args.size() == 3 && args[1] == "--symbols") {
		generate_symbols(args[2]);
		return ARG_EXIT;
	}

	return ARG_SUCCESS;
}

/**
 * @brief BinaryInfo::generateSymbolFile
 * @param filename
 * @param symbol_file
 * @return
 */
bool BinaryInfo::generateSymbolFile(const QString &filename, const QString &symbol_file) {

	std::ofstream file(qPrintable(symbol_file));
	if (file) {
		if (generate_symbols(filename, file)) {
			return true;
		}
	}

	return false;
}

}
