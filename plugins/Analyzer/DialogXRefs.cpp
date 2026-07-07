/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DialogXRefs.h"

namespace AnalyzerPlugin {

/**
 * @brief Constructs the cross-references dialog and sets up its UI.
 *
 * @param parent The parent widget for this dialog.
 * @param f The window flags for this dialog.
 */
DialogXRefs::DialogXRefs(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {
	ui.setupUi(this);
}

/**
 * @brief Jumps the disassembly view to the source address of the double-clicked cross-reference entry.
 *
 * @param item The double-clicked item in the cross-references list.
 */
void DialogXRefs::on_listReferences_itemDoubleClicked(QListWidgetItem *item) {

	edb::address_t site = item->data(Qt::UserRole).toULongLong();
	edb::v1::jump_to_address(site);
}

/**
 * @brief Adds a new cross-reference to the dialog, formatting the source symbol name and target address.
 *
 * @param ref A pair containing the source and target addresses of the cross-reference.
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
