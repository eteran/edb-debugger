/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "BinarySearcher.h"
#include "DialogAsciiString.h"
#include "DialogBinaryString.h"
#include "edb.h"
#include <QMenu>

namespace BinarySearcherPlugin {

/**
 * @brief BinarySearcher::BinarySearcher
 * @param parent
 */
BinarySearcher::BinarySearcher(QObject *parent)
	: QObject(parent) {
}

/**
 * @brief BinarySearcher::menu
 * @param parent
 * @return
 */
QMenu *BinarySearcher::menu(QWidget *parent) {

	Q_ASSERT(parent);

	if (!menu_) {
		menu_ = new QMenu(tr("BinarySearcher"), parent);
		menu_->addAction(tr("&Binary String Search"), this, SLOT(showMenu()), QKeySequence(tr("Ctrl+F")));
	}

	return menu_;
}

/**
 * @brief BinarySearcher::stackContextMenu
 * @return
 */
QList<QAction *> BinarySearcher::stackContextMenu() {

	QList<QAction *> ret;

	auto action_find = new QAction(tr("&Find ASCII String"), this);
	connect(action_find, &QAction::triggered, this, &BinarySearcher::mnuStackFindAscii);
	ret << action_find;

	return ret;
}

/**
 * @brief BinarySearcher::showMenu
 */
void BinarySearcher::showMenu() {
	static auto dialog = new DialogBinaryString(edb::v1::debugger_ui);
	dialog->show();
}

/**
 * @brief BinarySearcher::mnuStackFindAscii
 */
void BinarySearcher::mnuStackFindAscii() {
	static auto dialog = new DialogAsciiString(edb::v1::debugger_ui);
	dialog->show();
}

}
