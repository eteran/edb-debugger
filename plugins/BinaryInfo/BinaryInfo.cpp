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
#include "DialogHeader.h"
#include "ELF32.h"
#include "ELF64.h"
#include "IBinary.h"
#include "ISymbolManager.h"
#include "PE32.h"
#include "edb.h"
#include "symbols.h"
#include "OptionsPage.h"

#include <QDebug>
#include <QMenu>

#include <fstream>
#include <memory>

namespace BinaryInfo {
namespace {

//------------------------------------------------------------------------------
// Name: create_binary_info_elf32
// Desc:
//------------------------------------------------------------------------------
std::unique_ptr<IBinary> create_binary_info_elf32(const IRegion::pointer &region) {
	return std::unique_ptr<IBinary>(new ELF32(region));
}

//------------------------------------------------------------------------------
// Name: create_binary_info_elf64
// Desc:
//------------------------------------------------------------------------------
std::unique_ptr<IBinary> create_binary_info_elf64(const IRegion::pointer &region) {
	return std::unique_ptr<IBinary>(new ELF64(region));
}

//------------------------------------------------------------------------------
// Name: create_binary_info_pe32
// Desc:
//------------------------------------------------------------------------------
std::unique_ptr<IBinary> create_binary_info_pe32(const IRegion::pointer &region) {
	return std::unique_ptr<IBinary>(new PE32(region));
}

}

//------------------------------------------------------------------------------
// Name: BinaryInfo
// Desc:
//------------------------------------------------------------------------------
BinaryInfo::BinaryInfo() : menu_(0) {
}

//------------------------------------------------------------------------------
// Name: private_init
// Desc:
//------------------------------------------------------------------------------
void BinaryInfo::private_init() {
	edb::v1::register_binary_info(create_binary_info_elf32);
	edb::v1::register_binary_info(create_binary_info_elf64);
	edb::v1::register_binary_info(create_binary_info_pe32);
	edb::v1::symbol_manager().set_symbol_generator(this);
}

QWidget* BinaryInfo::options_page() {
	return new OptionsPage;
}

//------------------------------------------------------------------------------
// Name: menu
// Desc:
//------------------------------------------------------------------------------
QMenu *BinaryInfo::menu(QWidget *parent) {

	Q_ASSERT(parent);

	if(!menu_) {
		menu_ = new QMenu(tr("Binary Info"), parent);
		menu_->addAction(tr("&Explore Binary Header"), this, SLOT(explore_header()));
	}

	return menu_;
}

//------------------------------------------------------------------------------
// Name: explore_header
// Desc:
//------------------------------------------------------------------------------
void BinaryInfo::explore_header() {
	static auto dialog = new DialogHeader(edb::v1::debugger_ui);
	dialog->show();
}

//------------------------------------------------------------------------------
// Name: extra_arguments
// Desc:
//------------------------------------------------------------------------------
QString BinaryInfo::extra_arguments() const {
	return " --symbols <filename>      : generate symbols for <filename> and exit";
}

//------------------------------------------------------------------------------
// Name: parse_argments
// Desc:
//------------------------------------------------------------------------------
IPlugin::ArgumentStatus BinaryInfo::parse_argments(QStringList &args) {

	if(args.size() == 3 && args[1] == "--symbols") {
		generate_symbols(args[2]);
		return ARG_EXIT;
	}

	return ARG_SUCCESS;
}

//------------------------------------------------------------------------------
// Name: generate_symbol_file
// Desc:
//------------------------------------------------------------------------------
bool BinaryInfo::generate_symbol_file(const QString &filename, const QString &symbol_file) {

	std::ofstream file(qPrintable(symbol_file));
	if(file) {
		if(generate_symbols(filename, file)) {
			return true;
		}
	}

	return false;
}

#if QT_VERSION < 0x050000
Q_EXPORT_PLUGIN2(BinaryInfo, BinaryInfo)
#endif

}
