/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "OpcodeSearcher.h"
#include "DialogOpcodes.h"
#include "edb.h"
#include <QMenu>

namespace OpcodeSearcherPlugin {

/**
 * @brief Constructs an OpcodeSearcher object with the specified parent QObject.
 *
 * @param parent The parent QObject for this plugin.
 */
OpcodeSearcher::OpcodeSearcher(QObject *parent)
	: QObject(parent) {
}

/**
 * @brief Destructs the OpcodeSearcher object and frees the opcode search dialog.
 */
OpcodeSearcher::~OpcodeSearcher() {
	delete dialog_;
}

/**
 * @brief Returns the menu for the OpcodeSearcher plugin, creating it if it doesn't already exist.
 *
 * @param parent The parent widget for the menu.
 * @return A pointer to the QMenu object.
 */
QMenu *OpcodeSearcher::menu(QWidget *parent) {

	Q_ASSERT(parent);

	if (!menu_) {
		menu_ = new QMenu(tr("OpcodeSearcher"), parent);
		menu_->addAction(tr("&Opcode Search"), this, &OpcodeSearcher::showMenu, QKeySequence(tr("Ctrl+O")));
	}

	return menu_;
}

/**
 * @brief Shows the Opcode Search menu. If the dialog does not already exist, it is created and then shown.
 */
void OpcodeSearcher::showMenu() {

	if (!dialog_) {
		dialog_ = new DialogOpcodes(edb::v1::debugger_ui);
	}

	dialog_->show();
}

}
