/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "References.h"
#include "DialogReferences.h"
#include "edb.h"
#include <QMenu>

namespace ReferencesPlugin {

/**
 * @brief References::References
 * @param parent
 */
References::References(QObject *parent)
	: QObject(parent) {
}

/**
 * @brief References::~References
 */
References::~References() {
	delete dialog_;
}

/**
 * @brief References::menu
 * @param parent
 * @return
 */
QMenu *References::menu(QWidget *parent) {

	Q_ASSERT(parent);

	if (!menu_) {
		menu_ = new QMenu(tr("Reference Searcher"), parent);
		menu_->addAction(tr("&Reference Search"), this, SLOT(showMenu()), QKeySequence(tr("Ctrl+R")));
	}

	return menu_;
}

/**
 * @brief References::showMenu
 */
void References::showMenu() {

	if (!dialog_) {
		dialog_ = new DialogReferences(edb::v1::debugger_ui);
	}

	dialog_->show();
}

}
