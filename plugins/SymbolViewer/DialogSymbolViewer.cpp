/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DialogSymbolViewer.h"
#include "Configuration.h"
#include "IDebugger.h"
#include "ISymbolManager.h"
#include "Symbol.h"
#include "Util.h"
#include "edb.h"

#include <QMenu>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStringListModel>

namespace SymbolViewerPlugin {

/**
 * @brief Constructs a DialogSymbolViewer instance with the specified parent and window flags.
 *
 * @param parent The parent widget.
 * @param f The window flags.
 */
DialogSymbolViewer::DialogSymbolViewer(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {

	ui.setupUi(this);

	buttonRefresh_ = new QPushButton(QIcon::fromTheme(QStringLiteral("view-refresh")), tr("Refresh"));
	connect(buttonRefresh_, &QPushButton::clicked, this, [this]() {
		buttonRefresh_->setEnabled(false);
		doFind();
		buttonRefresh_->setEnabled(true);
	});

	ui.buttonBox->addButton(buttonRefresh_, QDialogButtonBox::ActionRole);

	ui.listView->setContextMenuPolicy(Qt::CustomContextMenu);

	model_       = new QStringListModel(this);
	filterModel_ = new QSortFilterProxyModel(this);

	filterModel_->setFilterKeyColumn(0);
	filterModel_->setSourceModel(model_);
	ui.listView->setModel(filterModel_);
	ui.listView->setUniformItemSizes(true);

	connect(ui.txtSearch, &QLineEdit::textChanged, filterModel_, &QSortFilterProxyModel::setFilterFixedString);
}

/**
 * @brief Handles the double-click event on the list view in the DialogSymbolViewer instance.
 * This function is called when an item in the list view is double-clicked and follows
 * the found item in the data view.
 *
 * @param index The index of the double-clicked item in the list view.
 */
void DialogSymbolViewer::on_listView_doubleClicked(const QModelIndex &index) {

	const QString s = index.data().toString();

	if (const Result<edb::address_t, QString> addr = edb::v1::string_to_address(s.split(":")[0])) {
		const std::optional<Symbol> sym = edb::v1::symbol_manager().find(*addr);

		if (sym && sym->isCode()) {
			edb::v1::jump_to_address(*addr);
		} else {
			edb::v1::dump_data(*addr, false);
		}
	}
}

/**
 * @brief Handles the custom context menu request event for the list view in the DialogSymbolViewer instance.
 *
 * @param pos The position of the right-click event.
 */
void DialogSymbolViewer::on_listView_customContextMenuRequested(const QPoint &pos) {

	const QModelIndex index = ui.listView->indexAt(pos);
	if (index.isValid()) {

		const QString s = index.data().toString();

		if (const Result<edb::address_t, QString> addr = edb::v1::string_to_address(s.split(":")[0])) {

			QMenu menu;
			QAction *const action1 = menu.addAction(tr("&Follow In Disassembly"), this, &DialogSymbolViewer::mnuFollowInCPU);
			QAction *const action2 = menu.addAction(tr("&Follow In Dump"), this, &DialogSymbolViewer::mnuFollowInDump);
			QAction *const action3 = menu.addAction(tr("&Follow In Dump (New Tab)"), this, &DialogSymbolViewer::mnuFollowInDumpNewTab);
			QAction *const action4 = menu.addAction(tr("&Follow In Stack"), this, &DialogSymbolViewer::mnuFollowInStack);

			action1->setData(addr->toQVariant());
			action2->setData(addr->toQVariant());
			action3->setData(addr->toQVariant());
			action4->setData(addr->toQVariant());

			menu.exec(ui.listView->mapToGlobal(pos));
		}
	}
}

/**
 * @brief Handles the "Follow In Dump" action in the DialogSymbolViewer instance.
 *
 * @param address The address to dump.
 */
void DialogSymbolViewer::mnuFollowInDump() {
	if (auto action = qobject_cast<QAction *>(sender())) {
		const edb::address_t address = action->data().toULongLong();
		edb::v1::dump_data(address, false);
	}
}

/**
 * @brief Handles the "Follow In Dump (New Tab)" action in the DialogSymbolViewer instance.
 *
 * @param address The address to dump in a new tab.
 */
void DialogSymbolViewer::mnuFollowInDumpNewTab() {
	if (auto action = qobject_cast<QAction *>(sender())) {
		const edb::address_t address = action->data().toULongLong();
		edb::v1::dump_data(address, true);
	}
}

/**
 * @brief Handles the "Follow In Stack" action in the DialogSymbolViewer instance.
 *
 * @param address The address to dump in the stack view.
 */
void DialogSymbolViewer::mnuFollowInStack() {
	if (auto action = qobject_cast<QAction *>(sender())) {
		const edb::address_t address = action->data().toULongLong();
		edb::v1::dump_stack(address, false);
	}
}

/**
 * @brief Handles the "Follow In CPU" action in the DialogSymbolViewer instance.
 *
 * @param address The address to jump to in the CPU view.
 */
void DialogSymbolViewer::mnuFollowInCPU() {
	if (auto action = qobject_cast<QAction *>(sender())) {
		const edb::address_t address = action->data().toULongLong();
		edb::v1::jump_to_address(address);
	}
}

/**
 * @brief Performs a search for symbols and updates the list view in the DialogSymbolViewer instance.
 * This function retrieves the list of symbols from the symbol manager and populates the list view with the results.
 */
void DialogSymbolViewer::doFind() {

	const std::vector<Symbol> symbols = edb::v1::symbol_manager().symbols();
	QStringList results;
	results.reserve(static_cast<int>(symbols.size()));
	for (const Symbol &sym : symbols) {
		results.push_back(QStringLiteral("%1: %2").arg(edb::v1::format_pointer(sym.address), sym.name));
	}

	model_->setStringList(results);
}

/**
 * @brief Handles the show event for the DialogSymbolViewer instance.
 */
void DialogSymbolViewer::showEvent(QShowEvent * /*event*/) {
	buttonRefresh_->setEnabled(false);
	doFind();
	buttonRefresh_->setEnabled(true);
}

}
