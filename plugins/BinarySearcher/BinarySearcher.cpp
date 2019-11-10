/*
Copyright (C) 2006 - 2015 Evan Teran
                          evan.teran@gmail.com

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
