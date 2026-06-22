/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ROPTool.h"
#include "DialogROPTool.h"
#include "edb.h"
#include <QMenu>

namespace ROPToolPlugin {

/**
 * @brief Constructor for the ROPTool plugin.
 *
 * @param parent The parent QObject for this plugin.
 */
ROPTool::ROPTool(QObject *parent)
	: QObject(parent) {
}

/**
 * @brief Destructor for the ROPTool plugin.
 */
ROPTool::~ROPTool() {
	delete dialog_;
}

/**
 * @brief Returns the context menu for this plugin, creating it if it doesn't already exist.
 *
 * @param parent The parent widget for the menu.
 * @return A pointer to the created menu.
 */
QMenu *ROPTool::menu(QWidget *parent) {

	Q_ASSERT(parent);

	if (!menu_) {
		menu_ = new QMenu(tr("ROPTool"), parent);
		menu_->addAction(tr("&ROP Tool"), this, SLOT(showMenu()), QKeySequence(tr("Ctrl+Alt+R")));
	}

	return menu_;
}

/**
 * @brief Slot to show the ROP Tool dialog. If the dialog does not already exist, it is created and then shown.
 */
void ROPTool::showMenu() {

	if (!dialog_) {
		dialog_ = new DialogROPTool(edb::v1::debugger_ui);
	}

	dialog_->show();
}

}
