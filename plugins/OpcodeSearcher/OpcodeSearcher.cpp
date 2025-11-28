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
 * @brief OpcodeSearcher::OpcodeSearcher
 * @param parent
 */
OpcodeSearcher::OpcodeSearcher(QObject *parent)
	: QObject(parent) {
}

/**
 * @brief OpcodeSearcher::~OpcodeSearcher
 */
OpcodeSearcher::~OpcodeSearcher() {
	delete dialog_;
}

/**
 * @brief OpcodeSearcher::menu
 * @param parent
 * @return
 */
QMenu *OpcodeSearcher::menu(QWidget *parent) {

	Q_ASSERT(parent);

	if (!menu_) {
		menu_ = new QMenu(tr("OpcodeSearcher"), parent);
		menu_->addAction(tr("&Opcode Search"), this, SLOT(showMenu()), QKeySequence(tr("Ctrl+O")));
	}

	return menu_;
}

/**
 * @brief OpcodeSearcher::showMenu
 */
void OpcodeSearcher::showMenu() {

	if (!dialog_) {
		dialog_ = new DialogOpcodes(edb::v1::debugger_ui);
	}

	dialog_->show();
}

}
