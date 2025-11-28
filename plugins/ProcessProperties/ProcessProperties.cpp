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
 * @brief ProcessProperties::ProcessProperties
 * @param parent
 */
ProcessProperties::ProcessProperties(QObject *parent)
	: QObject(parent) {

	dialog_ = new DialogProcessProperties(edb::v1::debugger_ui);
}

/**
 * @brief ProcessProperties::~ProcessProperties
 */
ProcessProperties::~ProcessProperties() {
#if 0
	delete dialog_;
#endif
}

/**
 * @brief ProcessProperties::menu
 * @param parent
 * @return
 */
QMenu *ProcessProperties::menu(QWidget *parent) {

	Q_ASSERT(parent);

	if (!menu_) {
		menu_ = new QMenu(tr("Process Properties"), parent);
		menu_->addAction(tr("&Process Properties"), this, SLOT(showMenu()), QKeySequence(tr("Ctrl+P")));
		menu_->addAction(tr("Process &Strings"), dialog_, SLOT(on_btnStrings_clicked()), QKeySequence(tr("Ctrl+S")));
	}

	return menu_;
}

/**
 * @brief ProcessProperties::showMenu
 */
void ProcessProperties::showMenu() {
	dialog_->show();
}

}
