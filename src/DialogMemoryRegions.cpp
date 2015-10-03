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
#include "edb.h"
#include "MemoryRegions.h"

#include <QHeaderView>
#include <QString>
#include <QMenu>
#include <QSortFilterProxyModel>
#include <QItemSelectionModel>
#include <QDebug>

#include "ui_DialogMemoryRegions.h"

//------------------------------------------------------------------------------
// Name: DialogMemoryRegions
// Desc:
//------------------------------------------------------------------------------
DialogMemoryRegions::DialogMemoryRegions(QWidget *parent) : QDialog(parent), ui(new Ui::DialogMemoryRegions) {
	ui->setupUi(this);

	ui->regions_table->verticalHeader()->hide();
#if QT_VERSION >= 0x050000
	ui->regions_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
#else
	ui->regions_table->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
#endif
	filter_model_ = new QSortFilterProxyModel(this);
	connect(ui->filter, SIGNAL(textChanged(const QString &)), filter_model_, SLOT(setFilterFixedString(const QString &)));
}

//------------------------------------------------------------------------------
// Name: ~DialogMemoryRegions
// Desc:
//------------------------------------------------------------------------------
DialogMemoryRegions::~DialogMemoryRegions() {
	delete ui;
}

//------------------------------------------------------------------------------
// Name: showEvent
// Desc:
//------------------------------------------------------------------------------
void DialogMemoryRegions::showEvent(QShowEvent *) {

	filter_model_->setFilterKeyColumn(3);
	filter_model_->setSourceModel(&edb::v1::memory_regions());
	ui->regions_table->setModel(filter_model_);
}

//------------------------------------------------------------------------------
// Name: on_regions_table_customContextMenuRequested
// Desc:
//------------------------------------------------------------------------------
void DialogMemoryRegions::on_regions_table_customContextMenuRequested(const QPoint &pos) {

	QMenu menu;
	auto access_menu = new QMenu(tr("Set Access"), this);
	access_menu->addAction(tr("No Access"), this, SLOT(set_access_none()));   // ---
	access_menu->addAction(tr("Read Only"), this, SLOT(set_access_r()));      // r--
	access_menu->addAction(tr("Write Only"), this, SLOT(set_access_w()));     // -w-
	access_menu->addAction(tr("Execute Only"), this, SLOT(set_access_x()));   // --x
	access_menu->addAction(tr("Read/Write"), this, SLOT(set_access_rw()));    // rw-
	access_menu->addAction(tr("Read/Execute"), this, SLOT(set_access_rx()));  // r-x
	access_menu->addAction(tr("Write/Execute"), this, SLOT(set_access_wx())); // -wx
	access_menu->addAction(tr("Full Access"), this, SLOT(set_access_rwx()));  // rwx

	menu.addMenu(access_menu);
	menu.addSeparator();
	menu.addAction(tr("View in &CPU"), this, SLOT(view_in_cpu()));
	menu.addAction(tr("View in &Stack"), this, SLOT(view_in_stack()));
	menu.addAction(tr("View in &Dump"), this, SLOT(view_in_dump()));
	menu.exec(ui->regions_table->mapToGlobal(pos));
}

//------------------------------------------------------------------------------
// Name: selected_region
// Desc:
//------------------------------------------------------------------------------
IRegion::pointer DialogMemoryRegions::selected_region() const {
	const QItemSelectionModel *const selModel = ui->regions_table->selectionModel();
	const QModelIndexList sel = selModel->selectedRows();
	IRegion::pointer ret;
	if(sel.size() == 1) {
		const QModelIndex index = filter_model_->mapToSource(sel[0]);
		ret = *reinterpret_cast<IRegion::pointer *>(index.internalPointer());
	}
	
	return ret;
}

//------------------------------------------------------------------------------
// Name: set_permissions
// Desc:
//------------------------------------------------------------------------------
void DialogMemoryRegions::set_permissions(bool read, bool write, bool execute) {
	if(IRegion::pointer region = selected_region()) {
		region->set_permissions(read, write, execute);
		edb::v1::memory_regions().sync();
	}
}

//------------------------------------------------------------------------------
// Name: set_access_none
// Desc:
//------------------------------------------------------------------------------
void DialogMemoryRegions::set_access_none() {
	set_permissions(false, false, false);
}

//------------------------------------------------------------------------------
// Name: set_access_r
// Desc:
//------------------------------------------------------------------------------
void DialogMemoryRegions::set_access_r() {
	set_permissions(true, false, false);
}

//------------------------------------------------------------------------------
// Name: set_access_w
// Desc:
//------------------------------------------------------------------------------
void DialogMemoryRegions::set_access_w() {
	set_permissions(false, true, false);
}

//------------------------------------------------------------------------------
// Name: set_access_x
// Desc:
//------------------------------------------------------------------------------
void DialogMemoryRegions::set_access_x() {
	set_permissions(false, false, true);
}

//------------------------------------------------------------------------------
// Name: set_access_rw
// Desc:
//------------------------------------------------------------------------------
void DialogMemoryRegions::set_access_rw() {
	set_permissions(true, true, false);
}

//------------------------------------------------------------------------------
// Name: set_access_rx
// Desc:
//------------------------------------------------------------------------------
void DialogMemoryRegions::set_access_rx() {
	set_permissions(true, false, true);
}

//------------------------------------------------------------------------------
// Name: set_access_wx
// Desc:
//------------------------------------------------------------------------------
void DialogMemoryRegions::set_access_wx() {
	set_permissions(false, true, true);
}

//------------------------------------------------------------------------------
// Name: set_access_rwx
// Desc:
//------------------------------------------------------------------------------
void DialogMemoryRegions::set_access_rwx() {
	set_permissions(true, true, true);
}

//------------------------------------------------------------------------------
// Name: view_in_cpu
// Desc:
//------------------------------------------------------------------------------
void DialogMemoryRegions::view_in_cpu() {
	if(IRegion::pointer region = selected_region()) {
		edb::v1::jump_to_address(region->start());
	}
}

//------------------------------------------------------------------------------
// Name: view_in_stack
// Desc:
//------------------------------------------------------------------------------
void DialogMemoryRegions::view_in_stack() {
	if(IRegion::pointer region = selected_region()) {
		edb::v1::dump_stack(region->start(), true);
	}
}

//------------------------------------------------------------------------------
// Name: view_in_dump
// Desc:
//------------------------------------------------------------------------------
void DialogMemoryRegions::view_in_dump() {
	if(IRegion::pointer region = selected_region()) {
		edb::v1::dump_data(region->start(), true);
	}
}

//------------------------------------------------------------------------------
// Name: on_regions_table_doubleClicked
// Desc:
//------------------------------------------------------------------------------
void DialogMemoryRegions::on_regions_table_doubleClicked(const QModelIndex &index) {
	Q_UNUSED(index);
	if(IRegion::pointer region = selected_region()) {
		edb::v1::dump_data(region->start(), true);
	}
}

