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

#include "DialogMemoryRegions.h"
#include "IDebugger.h"
#include "IRegion.h"
#include "MemoryRegions.h"
#include "edb.h"

#include <QDebug>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QMenu>
#include <QSortFilterProxyModel>
#include <QString>

//------------------------------------------------------------------------------
// Name: DialogMemoryRegions
// Desc:
//------------------------------------------------------------------------------
DialogMemoryRegions::DialogMemoryRegions(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {
	ui.setupUi(this);

	ui.regions_table->verticalHeader()->hide();
	ui.regions_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
	filterModel_ = new QSortFilterProxyModel(this);

	connect(ui.filter, &QLineEdit::textChanged, filterModel_, &QSortFilterProxyModel::setFilterFixedString);
}

//------------------------------------------------------------------------------
// Name: showEvent
// Desc:
//------------------------------------------------------------------------------
void DialogMemoryRegions::showEvent(QShowEvent *) {

	filterModel_->setFilterKeyColumn(3);
	filterModel_->setSourceModel(&edb::v1::memory_regions());
	ui.regions_table->setModel(filterModel_);
}

//------------------------------------------------------------------------------
// Name: on_regions_table_customContextMenuRequested
// Desc:
//------------------------------------------------------------------------------
void DialogMemoryRegions::on_regions_table_customContextMenuRequested(const QPoint &pos) {

	QMenu menu;
	auto access_menu = new QMenu(tr("Set Access"), this);
	access_menu->addAction(tr("No Access"), this, SLOT(setAccessNone()));   // ---
	access_menu->addAction(tr("Read Only"), this, SLOT(setAccessR()));      // r--
	access_menu->addAction(tr("Write Only"), this, SLOT(setAccessW()));     // -w-
	access_menu->addAction(tr("Execute Only"), this, SLOT(setAccessX()));   // --x
	access_menu->addAction(tr("Read/Write"), this, SLOT(setAccessRW()));    // rw-
	access_menu->addAction(tr("Read/Execute"), this, SLOT(setAccessRX()));  // r-x
	access_menu->addAction(tr("Write/Execute"), this, SLOT(setAccessWX())); // -wx
	access_menu->addAction(tr("Full Access"), this, SLOT(setAccessRWX()));  // rwx

	menu.addMenu(access_menu);
	menu.addSeparator();
	menu.addAction(tr("View in &CPU"), this, SLOT(viewInCpu()));
	menu.addAction(tr("View in &Stack"), this, SLOT(viewInStack()));
	menu.addAction(tr("View in &Dump"), this, SLOT(viewInDump()));
	menu.exec(ui.regions_table->mapToGlobal(pos));
}

//------------------------------------------------------------------------------
// Name: selected_region
// Desc:
//------------------------------------------------------------------------------
std::shared_ptr<IRegion> DialogMemoryRegions::selectedRegion() const {
	const QItemSelectionModel *const selModel = ui.regions_table->selectionModel();
	const QModelIndexList sel                 = selModel->selectedRows();

	if (sel.size() == 1) {
		const QModelIndex index = filterModel_->mapToSource(sel[0]);
		return *reinterpret_cast<std::shared_ptr<IRegion> *>(index.internalPointer());
	}

	return {};
}

//------------------------------------------------------------------------------
// Name: set_permissions
// Desc:
//------------------------------------------------------------------------------
void DialogMemoryRegions::setPermissions(bool read, bool write, bool execute) {
	if (std::shared_ptr<IRegion> region = selectedRegion()) {
		region->setPermissions(read, write, execute);
		edb::v1::memory_regions().sync();
	}
}

//------------------------------------------------------------------------------
// Name: set_access_none
// Desc:
//------------------------------------------------------------------------------
void DialogMemoryRegions::setAccessNone() {
	setPermissions(false, false, false);
}

//------------------------------------------------------------------------------
// Name: set_access_r
// Desc:
//------------------------------------------------------------------------------
void DialogMemoryRegions::setAccessR() {
	setPermissions(true, false, false);
}

//------------------------------------------------------------------------------
// Name: set_access_w
// Desc:
//------------------------------------------------------------------------------
void DialogMemoryRegions::setAccessW() {
	setPermissions(false, true, false);
}

//------------------------------------------------------------------------------
// Name: set_access_x
// Desc:
//------------------------------------------------------------------------------
void DialogMemoryRegions::setAccessX() {
	setPermissions(false, false, true);
}

//------------------------------------------------------------------------------
// Name: set_access_rw
// Desc:
//------------------------------------------------------------------------------
void DialogMemoryRegions::setAccessRW() {
	setPermissions(true, true, false);
}

//------------------------------------------------------------------------------
// Name: set_access_rx
// Desc:
//------------------------------------------------------------------------------
void DialogMemoryRegions::setAccessRX() {
	setPermissions(true, false, true);
}

//------------------------------------------------------------------------------
// Name: set_access_wx
// Desc:
//------------------------------------------------------------------------------
void DialogMemoryRegions::setAccessWX() {
	setPermissions(false, true, true);
}

//------------------------------------------------------------------------------
// Name: set_access_rwx
// Desc:
//------------------------------------------------------------------------------
void DialogMemoryRegions::setAccessRWX() {
	setPermissions(true, true, true);
}

//------------------------------------------------------------------------------
// Name: view_in_cpu
// Desc:
//------------------------------------------------------------------------------
void DialogMemoryRegions::viewInCpu() {
	if (std::shared_ptr<IRegion> region = selectedRegion()) {
		edb::v1::jump_to_address(region->start());
	}
}

//------------------------------------------------------------------------------
// Name: view_in_stack
// Desc:
//------------------------------------------------------------------------------
void DialogMemoryRegions::viewInStack() {
	if (std::shared_ptr<IRegion> region = selectedRegion()) {
		edb::v1::dump_stack(region->start(), true);
	}
}

//------------------------------------------------------------------------------
// Name: view_in_dump
// Desc:
//------------------------------------------------------------------------------
void DialogMemoryRegions::viewInDump() {
	if (std::shared_ptr<IRegion> region = selectedRegion()) {
		edb::v1::dump_data(region->start(), true);
	}
}

//------------------------------------------------------------------------------
// Name: on_regions_table_doubleClicked
// Desc:
//------------------------------------------------------------------------------
void DialogMemoryRegions::on_regions_table_doubleClicked(const QModelIndex &index) {
	Q_UNUSED(index)
	if (std::shared_ptr<IRegion> region = selectedRegion()) {
		if (region->executable()) {
			edb::v1::jump_to_address(region->start());
		} else {
			edb::v1::dump_data(region->start(), true);
		}
	}
}
