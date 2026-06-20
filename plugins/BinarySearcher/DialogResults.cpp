/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DialogResults.h"
#include "edb.h"

namespace BinarySearcherPlugin {

/**
 * @brief Constructs the search results dialog and sets up its UI.
 *
 * @param parent
 * @param f
 */
DialogResults::DialogResults(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {

	ui.setupUi(this);
}

/**
 * @brief Navigates to the double-clicked result address in the appropriate view (code, stack, or data).
 *
 * @param item
 */
void DialogResults::on_listWidget_itemDoubleClicked(QListWidgetItem *item) {
	const edb::address_t addr = item->data(Qt::UserRole).toULongLong();
	switch (static_cast<RegionType>(item->data(Qt::UserRole + 1).toInt())) {
	case RegionType::Code:
		edb::v1::jump_to_address(addr);
		break;
	case RegionType::Stack:
		edb::v1::dump_stack(addr, true);
		break;
	case RegionType::Data:
		edb::v1::dump_data(addr);
		break;
	}
}

/**
 * @brief Adds a search result entry with the given region type and address to the results list.
 *
 * @param address
 */
void DialogResults::addResult(RegionType region, edb::address_t address) {
	auto item = new QListWidgetItem(edb::v1::format_pointer(address));
	item->setData(Qt::UserRole, address.toQVariant());
	item->setData(Qt::UserRole + 1, static_cast<int>(region));
	ui.listWidget->addItem(item);
}

/**
 * @brief Returns the total number of results currently shown in the list.
 *
 * @return
 */
int DialogResults::resultCount() const {
	return ui.listWidget->count();
}

}
