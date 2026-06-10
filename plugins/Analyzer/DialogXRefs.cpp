/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DialogXRefs.h"

namespace AnalyzerPlugin {

/**
 * @brief DialogXRefs::DialogXRefs
 * @param parent
 */
DialogXRefs::DialogXRefs(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {
	ui.setupUi(this);
}

/**
 * @brief DialogXRefs::on_listReferences_itemDoubleClicked
 * @param item
 */
void DialogXRefs::on_listReferences_itemDoubleClicked(QListWidgetItem *item) {

	edb::address_t site = item->data(Qt::UserRole).toULongLong();
	edb::v1::jump_to_address(site);
}

/**
 * @brief DialogXRefs::addReference
 * @param ref
 */
void DialogXRefs::addReference(const std::pair<edb::address_t, edb::address_t> &ref) {

	auto it = references_.find(ref);
	if (it == references_.end()) {
		references_.insert(ref);
		const auto &[from, to] = ref;

		int offset;
		QString sym = edb::v1::find_function_symbol(from, from.toPointerString(), &offset);

		auto string = tr("%1. %2 -> %3").arg(ui.listReferences->count() + 1, 2, 10, QChar('0')).arg(sym).arg(to.toPointerString());

		auto item = new QListWidgetItem(string, ui.listReferences);
		item->setData(Qt::UserRole, static_cast<qlonglong>(from));
	}
}

}
