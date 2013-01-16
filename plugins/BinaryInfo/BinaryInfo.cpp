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
#include "Debugger.h"
#include "ELF32.h"
#include "ELF64.h"
#include "IBinary.h"
#include "PE32.h"
#include "DialogHeader.h"
#include <QMenu>
#include <QDebug>

namespace {
	IBinary *create_binary_info_elf32(const IRegion::pointer &region) {
		return new ELF32(region);
	}

	IBinary *create_binary_info_elf64(const IRegion::pointer &region) {
		return new ELF64(region);
	}
	
	IBinary *create_binary_info_pe32(const IRegion::pointer &region) {
		return new PE32(region);
	}
}

//------------------------------------------------------------------------------
// Name: BinaryInfo()
// Desc:
//------------------------------------------------------------------------------
BinaryInfo::BinaryInfo() : menu_(0) {
}

//------------------------------------------------------------------------------
// Name: BinaryInfo()
// Desc:
//------------------------------------------------------------------------------
void BinaryInfo::private_init() {
	edb::v1::register_binary_info(create_binary_info_elf32);
	edb::v1::register_binary_info(create_binary_info_elf64);
	edb::v1::register_binary_info(create_binary_info_pe32);
}

//------------------------------------------------------------------------------
// Name: menu(QWidget *parent)
// Desc:
//------------------------------------------------------------------------------
QMenu *BinaryInfo::menu(QWidget *parent) {
	if(menu_ == 0) {
		menu_ = new QMenu(tr("Binary Info"), parent);
		menu_->addAction(tr("&Explore Binary Header"), this, SLOT(explore_header()));
	}

	return menu_;
}

//------------------------------------------------------------------------------
// Name: menu(QWidget *parent)
// Desc:
//------------------------------------------------------------------------------
void BinaryInfo::explore_header() {
	static QDialog *dialog = new DialogHeader(edb::v1::debugger_ui);
	dialog->show();
}

Q_EXPORT_PLUGIN2(BinaryInfo, BinaryInfo)
