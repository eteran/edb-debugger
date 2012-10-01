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

#include "MemoryRegions.h"
#include "SymbolManager.h"
#include "IDebuggerCore.h"
#include "Util.h"
#include "State.h"
#include "Debugger.h"

#include <QMessageBox>

//------------------------------------------------------------------------------
// Name: MemoryRegions()
// Desc: constructor
//------------------------------------------------------------------------------
MemoryRegions::MemoryRegions() : QAbstractItemModel(0), pid_(0) {
}

//------------------------------------------------------------------------------
// Name: ~MemoryRegions()
// Desc: destructor
//------------------------------------------------------------------------------
MemoryRegions::~MemoryRegions() {
}

//------------------------------------------------------------------------------
// Name: set_pid(edb::pid_t pid)
// Desc:
//------------------------------------------------------------------------------
void MemoryRegions::set_pid(edb::pid_t pid) {
	pid_ = pid;
	regions_.clear();
	sync();
}

//------------------------------------------------------------------------------
// Name: clear()
// Desc:
//------------------------------------------------------------------------------
void MemoryRegions::clear() {
	pid_ = 0;
	regions_.clear();
}

//------------------------------------------------------------------------------
// Name: sync()
// Desc: reads a memory map file line by line
//------------------------------------------------------------------------------
void MemoryRegions::sync() {

	QList<MemoryRegion> regions;

	if(pid_ != 0) {
		if(HANDLE ph = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid_)) {
			edb::address_t addr = 0;
			LPVOID last_base    = reinterpret_cast<LPVOID>(-1);

			Q_FOREVER {
				MEMORY_BASIC_INFORMATION info;
				VirtualQueryEx(ph, reinterpret_cast<LPVOID>(addr), &info, sizeof(info));

				if(last_base == info.BaseAddress) {
					break;
				}

				last_base = info.BaseAddress;

				if(info.State == MEM_COMMIT) {

					const edb::address_t start = reinterpret_cast<edb::address_t>(info.BaseAddress);
					const edb::address_t end   = reinterpret_cast<edb::address_t>(info.BaseAddress) + info.RegionSize;
					const edb::address_t base  = reinterpret_cast<edb::address_t>(info.AllocationBase);
					const QString name         = QString();
					const IRegion::permissions_t permissions = info.Protect; // let MemoryRegion handle permissions and modifiers
					
					if(info.Type == MEM_IMAGE) {
						// set region.name to the module name
					}
					// get stack addresses, PEB, TEB, etc. and set name accordingly

					regions.push_back(MemoryRegion(start, end, base, name, permissions));
				}

				addr += info.RegionSize;
			}

			CloseHandle(ph);
		}
	}

	qSwap(regions_, regions);
	reset();
}

//------------------------------------------------------------------------------
// Name: find_region(edb::address_t address) const
// Desc:
//------------------------------------------------------------------------------
bool MemoryRegions::find_region(edb::address_t address) const {
	Q_FOREACH(const MemoryRegion &i, regions_) {
		if(i.contains(address)) {
			return true;
		}
	}
	return false;
}

//------------------------------------------------------------------------------
// Name: find_region(edb::address_t address, MemoryRegion &region) const
// Desc:
//------------------------------------------------------------------------------
bool MemoryRegions::find_region(edb::address_t address, MemoryRegion &region) const {
	Q_FOREACH(const MemoryRegion &i, regions_) {
		if(i.contains(address)) {
			region = i;
			return true;
		}
	}
	return false;
}

//------------------------------------------------------------------------------
// Name: data(const QModelIndex &index, int role) const
// Desc:
//------------------------------------------------------------------------------
QVariant MemoryRegions::data(const QModelIndex &index, int role) const {

	if(index.isValid() && role == Qt::DisplayRole) {

		const MemoryRegion &region = regions_[index.row()];

		switch(index.column()) {
		case 0:
			return edb::v1::format_pointer(region.start());
		case 1:
			return edb::v1::format_pointer(region.end());
		case 2:
			return QString("%1%2%3").arg(region.readable() ? 'r' : '-').arg(region.writable() ? 'w' : '-').arg(region.executable() ? 'x' : '-' );
		case 3:
			return region.name();
		}
	}

	return QVariant();
}

//------------------------------------------------------------------------------
// Name: index(int row, int column, const QModelIndex &parent) const
// Desc:
//------------------------------------------------------------------------------
QModelIndex MemoryRegions::index(int row, int column, const QModelIndex &parent) const {
	Q_UNUSED(parent);

	if(row >= regions_.size()) {
		return QModelIndex();
	}

	if(column >= 4) {
		return QModelIndex();
	}

	return createIndex(row, column, const_cast<MemoryRegion *>(&regions_[row]));
}

//------------------------------------------------------------------------------
// Name: parent(const QModelIndex &index) const
// Desc:
//------------------------------------------------------------------------------
QModelIndex MemoryRegions::parent(const QModelIndex &index) const {
	Q_UNUSED(index);
	return QModelIndex();
}

//------------------------------------------------------------------------------
// Name: rowCount(const QModelIndex &parent) const
// Desc:
//------------------------------------------------------------------------------
int MemoryRegions::rowCount(const QModelIndex &parent) const {
	Q_UNUSED(parent);
	return regions_.size();
}

//------------------------------------------------------------------------------
// Name: columnCount(const QModelIndex &parent) const
// Desc:
//------------------------------------------------------------------------------
int MemoryRegions::columnCount(const QModelIndex &parent) const {
	Q_UNUSED(parent);
	return 4;
}

//------------------------------------------------------------------------------
// Name: headerData(int section, Qt::Orientation orientation, int role) const
// Desc:
//------------------------------------------------------------------------------
QVariant MemoryRegions::headerData(int section, Qt::Orientation orientation, int role) const {
	if(role == Qt::DisplayRole && orientation == Qt::Horizontal) {
		switch(section) {
		case 0:
			return tr("Start Address");
		case 1:
			return tr("End Address");
		case 2:
			return tr("Permissions");
		case 3:
			return tr("Name");
		}
	}

	return QVariant();
}

