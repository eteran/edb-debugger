/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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

/**
 * @brief
 */
DialogMemoryRegions::DialogMemoryRegions(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {
	ui.setupUi(this);

	ui.regions_table->verticalHeader()->hide();
	ui.regions_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
	filterModel_ = new QSortFilterProxyModel(this);

	connect(ui.filter, &QLineEdit::textChanged, filterModel_, &QSortFilterProxyModel::setFilterFixedString);
}

/**
 * @brief
 */
void DialogMemoryRegions::showEvent(QShowEvent *) {

	filterModel_->setFilterKeyColumn(3);
	filterModel_->setSourceModel(&edb::v1::memory_regions());
	ui.regions_table->setModel(filterModel_);
}

/**
 * @brief
 */
void DialogMemoryRegions::on_regions_table_customContextMenuRequested(const QPoint &pos) {

	QMenu menu;
	auto access_menu = new QMenu(tr("Set Access"), this);
	access_menu->addAction(tr("No Access"), this, &DialogMemoryRegions::setAccessNone);   // ---
	access_menu->addAction(tr("Read Only"), this, &DialogMemoryRegions::setAccessR);      // r--
	access_menu->addAction(tr("Write Only"), this, &DialogMemoryRegions::setAccessW);     // -w-
	access_menu->addAction(tr("Execute Only"), this, &DialogMemoryRegions::setAccessX);   // --x
	access_menu->addAction(tr("Read/Write"), this, &DialogMemoryRegions::setAccessRW);    // rw-
	access_menu->addAction(tr("Read/Execute"), this, &DialogMemoryRegions::setAccessRX);  // r-x
	access_menu->addAction(tr("Write/Execute"), this, &DialogMemoryRegions::setAccessWX); // -wx
	access_menu->addAction(tr("Full Access"), this, &DialogMemoryRegions::setAccessRWX);  // rwx

	menu.addMenu(access_menu);
	menu.addSeparator();
	menu.addAction(tr("View in &CPU"), this, &DialogMemoryRegions::viewInCpu);
	menu.addAction(tr("View in &Stack"), this, &DialogMemoryRegions::viewInStack);
	menu.addAction(tr("View in &Dump"), this, &DialogMemoryRegions::viewInDump);
	menu.exec(ui.regions_table->mapToGlobal(pos));
}

/**
 * @brief
 */
std::shared_ptr<IRegion> DialogMemoryRegions::selectedRegion() const {
	const QItemSelectionModel *const selModel = ui.regions_table->selectionModel();
	const QModelIndexList sel                 = selModel->selectedRows();

	if (sel.size() == 1) {
		const QModelIndex index = filterModel_->mapToSource(sel[0]);
		return *reinterpret_cast<std::shared_ptr<IRegion> *>(index.internalPointer());
	}

	return {};
}

/**
 * @brief
 */
void DialogMemoryRegions::setPermissions(bool read, bool write, bool execute) {
	if (std::shared_ptr<IRegion> region = selectedRegion()) {
		region->setPermissions(read, write, execute);
		edb::v1::memory_regions().sync();
	}
}

/**
 * @brief
 */
void DialogMemoryRegions::setAccessNone() {
	setPermissions(false, false, false);
}

/**
 * @brief
 */
void DialogMemoryRegions::setAccessR() {
	setPermissions(true, false, false);
}

/**
 * @brief
 */
void DialogMemoryRegions::setAccessW() {
	setPermissions(false, true, false);
}

/**
 * @brief
 */
void DialogMemoryRegions::setAccessX() {
	setPermissions(false, false, true);
}

/**
 * @brief
 */
void DialogMemoryRegions::setAccessRW() {
	setPermissions(true, true, false);
}

/**
 * @brief
 */
void DialogMemoryRegions::setAccessRX() {
	setPermissions(true, false, true);
}

/**
 * @brief
 */
void DialogMemoryRegions::setAccessWX() {
	setPermissions(false, true, true);
}

/**
 * @brief
 */
void DialogMemoryRegions::setAccessRWX() {
	setPermissions(true, true, true);
}

/**
 * @brief
 */
void DialogMemoryRegions::viewInCpu() {
	if (std::shared_ptr<IRegion> region = selectedRegion()) {
		edb::v1::jump_to_address(region->start());
	}
}

/**
 * @brief
 */
void DialogMemoryRegions::viewInStack() {
	if (std::shared_ptr<IRegion> region = selectedRegion()) {
		edb::v1::dump_stack(region->start(), true);
	}
}

/**
 * @brief
 */
void DialogMemoryRegions::viewInDump() {
	if (std::shared_ptr<IRegion> region = selectedRegion()) {
		edb::v1::dump_data(region->start(), true);
	}
}

/**
 * @brief
 */
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
