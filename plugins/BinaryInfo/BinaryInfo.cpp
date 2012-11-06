/*
Copyright (C) 2006 - 2011 Evan Teran
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
#include "PE32.h"
#include "IBinary.h"

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
BinaryInfo::BinaryInfo() {
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
	Q_UNUSED(parent);
	return 0;
}

Q_EXPORT_PLUGIN2(BinaryInfo, BinaryInfo)
