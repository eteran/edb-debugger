/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "SymbolViewer.h"
#include "DialogSymbolViewer.h"
#include "edb.h"
#include <QMenu>

namespace SymbolViewerPlugin {

/**
 * @brief Constructor for the SymbolViewer plugin.
 *
 * @param parent The parent QObject for this plugin.
 */
SymbolViewer::SymbolViewer(QObject *parent)
	: QObject(parent) {
}

/**
 * @brief Destructor for the SymbolViewer plugin.
 */
SymbolViewer::~SymbolViewer() {
	delete dialog_;
}

/**
 * @brief Returns the context menu for this plugin, creating it if it doesn't already exist.
 *
 * @param parent The parent widget for the menu.
 * @return A pointer to the created menu.
 */
QMenu *SymbolViewer::menu(QWidget *parent) {

	Q_ASSERT(parent);

	if (!menu_) {
		menu_ = new QMenu(tr("SymbolViewer"), parent);
		menu_->addAction(tr("&Symbol Viewer"), this, SLOT(showMenu()), QKeySequence(tr("Ctrl+Alt+S")));
	}

	return menu_;
}

/**
 * @brief Slot to show the Symbol Viewer dialog. If the dialog does not already exist, it is created and then shown.
 */
void SymbolViewer::showMenu() {

	if (!dialog_) {
		dialog_ = new DialogSymbolViewer(edb::v1::debugger_ui);
	}

	dialog_->show();
}

}
