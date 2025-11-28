/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "FunctionFinder.h"
#include "DialogFunctions.h"
#include "edb.h"
#include <QMenu>

namespace FunctionFinderPlugin {

/**
 * @brief FunctionFinder::FunctionFinder
 * @param parent
 */
FunctionFinder::FunctionFinder(QObject *parent)
	: QObject(parent) {
}

/**
 * @brief FunctionFinder::~FunctionFinder
 */
FunctionFinder::~FunctionFinder() {
	delete dialog_;
}

/**
 * @brief FunctionFinder::menu
 * @param parent
 * @return
 */
QMenu *FunctionFinder::menu(QWidget *parent) {

	Q_ASSERT(parent);

	if (!menu_) {
		menu_ = new QMenu(tr("FunctionFinder"), parent);
		menu_->addAction(tr("&Function Finder"), this, SLOT(showMenu()), QKeySequence(tr("Ctrl+Shift+F")));
	}

	return menu_;
}

/**
 * @brief FunctionFinder::showMenu
 */
void FunctionFinder::showMenu() {

	if (!dialog_) {
		dialog_ = new DialogFunctions(edb::v1::debugger_ui);
	}

	dialog_->show();
}

}
