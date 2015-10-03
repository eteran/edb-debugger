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

#include "IDebugger.h"
#include "ISymbolManager.h"
#include "MemoryRegions.h"
#include "edb.h"

#include <QDebug>

//------------------------------------------------------------------------------
// Name: MemoryRegions
// Desc: constructor
//------------------------------------------------------------------------------
MemoryRegions::MemoryRegions() : QAbstractItemModel(0) {
}

//------------------------------------------------------------------------------
// Name: ~MemoryRegions
// Desc: destructor
//------------------------------------------------------------------------------
MemoryRegions::~MemoryRegions() {
}

//------------------------------------------------------------------------------
// Name: clear
// Desc:
//------------------------------------------------------------------------------
void MemoryRegions::clear() {
	regions_.clear();
}

//------------------------------------------------------------------------------
// Name: sync
// Desc: reads a memory map file line by line
//------------------------------------------------------------------------------
void MemoryRegions::sync() {

	beginResetModel();

	QList<IRegion::pointer> regions;
	
	if(edb::v1::debugger_core) {
		if(IProcess *process = edb::v1::debugger_core->process()) {
			regions = process->regions();
			for(const IRegion::pointer &region: regions) {
				// if the region has a name, is mapped starting
				// at the beginning of the file, and is executable, sounds
				// like a module mapping!
				if(!region->name().isEmpty()) {
					if(region->base() == 0) {
						if(region->executable()) {
							edb::v1::symbol_manager().load_symbol_file(region->name(), region->start());
						}
					}
				}
			}
		}
	}


	qSwap(regions_, regions);
	endResetModel();
}

//------------------------------------------------------------------------------
// Name: find_region
// Desc:
//------------------------------------------------------------------------------
IRegion::pointer MemoryRegions::find_region(edb::address_t address) const {

	for(const IRegion::pointer &i: regions_) {
		if(i->contains(address)) {
			return i;
		}
	}
	return IRegion::pointer();
}

//------------------------------------------------------------------------------
// Name: data
// Desc:
//------------------------------------------------------------------------------
QVariant MemoryRegions::data(const QModelIndex &index, int role) const {

	if(index.isValid() && role == Qt::DisplayRole) {

		const IRegion::pointer &region = regions_[index.row()];

		switch(index.column()) {
		case 0: return edb::v1::format_pointer(region->start());
		case 1: return edb::v1::format_pointer(region->end());
		case 2: return QString("%1%2%3").arg(region->readable() ? 'r' : '-').arg(region->writable() ? 'w' : '-').arg(region->executable() ? 'x' : '-');
		case 3: return region->name();
		}
	}

	return QVariant();
}

//------------------------------------------------------------------------------
// Name: index
// Desc:
//------------------------------------------------------------------------------
QModelIndex MemoryRegions::index(int row, int column, const QModelIndex &parent) const {
	Q_UNUSED(parent);

	if(row >= rowCount(parent) || column >= columnCount(parent)) {
		return QModelIndex();
	}

	return createIndex(row, column, const_cast<IRegion::pointer *>(&regions_[row]));
}

//------------------------------------------------------------------------------
// Name: parent
// Desc:
//------------------------------------------------------------------------------
QModelIndex MemoryRegions::parent(const QModelIndex &index) const {
	Q_UNUSED(index);
	return QModelIndex();
}

//------------------------------------------------------------------------------
// Name: rowCount
// Desc:
//------------------------------------------------------------------------------
int MemoryRegions::rowCount(const QModelIndex &parent) const {
	Q_UNUSED(parent);
	return regions_.size();
}

//------------------------------------------------------------------------------
// Name: columnCount
// Desc:
//------------------------------------------------------------------------------
int MemoryRegions::columnCount(const QModelIndex &parent) const {
	Q_UNUSED(parent);
	return 4;
}

//------------------------------------------------------------------------------
// Name: headerData
// Desc:
//------------------------------------------------------------------------------
QVariant MemoryRegions::headerData(int section, Qt::Orientation orientation, int role) const {
	if(role == Qt::DisplayRole && orientation == Qt::Horizontal) {
		switch(section) {
		case 0: return tr("Start Address");
		case 1: return tr("End Address");
		case 2: return tr("Permissions");
		case 3: return tr("Name");
		}
	}

	return QVariant();
}

