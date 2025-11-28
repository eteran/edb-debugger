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
 * @brief SymbolViewer::SymbolViewer
 * @param parent
 */
SymbolViewer::SymbolViewer(QObject *parent)
	: QObject(parent) {
}

/**
 * @brief SymbolViewer::~SymbolViewer
 */
SymbolViewer::~SymbolViewer() {
	delete dialog_;
}

/**
 * @brief SymbolViewer::menu
 * @param parent
 * @return
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
 * @brief SymbolViewer::showMenu
 */
void SymbolViewer::showMenu() {

	if (!dialog_) {
		dialog_ = new DialogSymbolViewer(edb::v1::debugger_ui);
	}

	dialog_->show();
}

}
