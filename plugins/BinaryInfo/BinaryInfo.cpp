/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
 * @brief Constructs the BinaryInfo plugin object.
 *
 * @param parent
 */
BinaryInfo::BinaryInfo(QObject *parent)
	: QObject(parent) {
}

/**
 * @brief Registers ELF32, ELF64, and PE32 binary parsers and sets this plugin as the symbol generator.
 */
void BinaryInfo::privateInit() {

	edb::v1::register_binary_info([](const std::shared_ptr<IRegion> &region) -> std::unique_ptr<IBinary> {
		return std::make_unique<ELF32>(region);
	});

	edb::v1::register_binary_info([](const std::shared_ptr<IRegion> &region) -> std::unique_ptr<IBinary> {
		return std::make_unique<ELF64>(region);
	});

	edb::v1::register_binary_info([](const std::shared_ptr<IRegion> &region) -> std::unique_ptr<IBinary> {
		return std::make_unique<PE32>(region);
	});

	edb::v1::symbol_manager().setSymbolGenerator(this);
}

/**
 * @brief Returns the plugin's configuration options widget.
 *
 * @return
 */
QWidget *BinaryInfo::optionsPage() {
	return new OptionsPage;
}

/**
 * @brief Creates and returns the Binary Info plugin menu, building it on first call.
 *
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
 * @brief Opens or raises the memory region browser dialog for exploring binary headers.
 */
void BinaryInfo::exploreHeader() {
	static auto dialog = new DialogRegions(edb::v1::debugger_ui);
	dialog->show();
}

/**
 * @brief Returns the extra command-line argument description string for the --symbols option.
 *
 * @return
 */
QString BinaryInfo::extraArguments() const {
	return " --symbols <filename>      : generate symbols for <filename> and exit";
}

/**
 * @brief Handles the --symbols command-line argument to generate symbol files, returning ARG_EXIT if processed.
 *
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
 * @brief Generates a symbol file for the given binary and writes it to the specified output path.
 *
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
