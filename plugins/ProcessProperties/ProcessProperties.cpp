/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ProcessProperties.h"
#include "DialogProcessProperties.h"
#include "edb.h"

#include <QMenu>

namespace ProcessPropertiesPlugin {

/**
 * @brief Constructs a ProcessProperties object with the specified parent QObject.
 *
 * @param parent The parent QObject for this plugin.
 */
ProcessProperties::ProcessProperties(QObject *parent)
	: QObject(parent) {

	dialog_ = new DialogProcessProperties(edb::v1::debugger_ui);
}

/**
 * @brief Destructs the ProcessProperties object and frees the process properties dialog.
 */
ProcessProperties::~ProcessProperties() {
#if 0
	delete dialog_;
#endif
}

/**
 * @brief Returns the menu for the ProcessProperties plugin.
 *
 * @param parent The parent widget for the menu.
 * @return The menu for the ProcessProperties plugin.
 */
QMenu *ProcessProperties::menu(QWidget *parent) {

	Q_ASSERT(parent);

	if (!menu_) {
		menu_ = new QMenu(tr("Process Properties"), parent);
		menu_->addAction(tr("&Process Properties"), this, &ProcessProperties::showMenu, QKeySequence(tr("Ctrl+P")));
		menu_->addAction(tr("Process &Strings"), dialog_, SLOT(on_btnStrings_clicked()), QKeySequence(tr("Ctrl+S")));
	}

	return menu_;
}

/**
 * @brief Shows the Process Properties menu.
 */
void ProcessProperties::showMenu() {
	dialog_->show();
}

}
