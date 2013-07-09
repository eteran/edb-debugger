/*
Copyright (C) 2006 - 2013 Evan Teran
                          eteran@alum.rit.edu

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
#include "edb.h"
#include "DialogHeader.h"
#include "ELF32.h"
#include "ELF64.h"
#include "IBinary.h"
#include "PE32.h"
#include <QDebug>
#include <QMenu>

namespace {

//------------------------------------------------------------------------------
// Name: create_binary_info_elf32
// Desc:
//------------------------------------------------------------------------------
IBinary *create_binary_info_elf32(const IRegion::pointer &region) {
	return new ELF32(region);
}

//------------------------------------------------------------------------------
// Name: create_binary_info_elf64
// Desc:
//------------------------------------------------------------------------------
IBinary *create_binary_info_elf64(const IRegion::pointer &region) {
	return new ELF64(region);
}

//------------------------------------------------------------------------------
// Name: create_binary_info_pe32
// Desc:
//------------------------------------------------------------------------------
IBinary *create_binary_info_pe32(const IRegion::pointer &region) {
	return new PE32(region);
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
	static QDialog *dialog = new DialogHeader(edb::v1::debugger_ui);
	dialog->show();
}

#if QT_VERSION < 0x050000
Q_EXPORT_PLUGIN2(BinaryInfo, BinaryInfo)
#endif
