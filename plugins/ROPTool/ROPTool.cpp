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
 * @brief ROPTool::ROPTool
 * @param parent
 */
ROPTool::ROPTool(QObject *parent)
	: QObject(parent) {
}

/**
 * @brief ROPTool::~ROPTool
 */
ROPTool::~ROPTool() {
	delete dialog_;
}

/**
 * @brief ROPTool::menu
 * @param parent
 * @return
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
 * @brief ROPTool::showMenu
 */
void ROPTool::showMenu() {

	if (!dialog_) {
		dialog_ = new DialogROPTool(edb::v1::debugger_ui);
	}

	dialog_->show();
}

}
