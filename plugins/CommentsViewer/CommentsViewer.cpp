/*
Copyright (C) 2020 Victorien molle <biche@biche.re>

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

#include "CommentsViewer.h"
#include "DialogCommentsViewer.h"
#include "edb.h"
#include <QMenu>


namespace CommentsViewerPlugin {

/**
 * @brief CommentsViewer::CommentsViewer
 * @param parent
 */
CommentsViewer::CommentsViewer(QObject *parent)
	: QObject(parent) {
}

/**
 * @brief CommentsViewer::~CommentsViewer
 */
CommentsViewer::~CommentsViewer() {
	delete dialog_;
}

/**
 * @brief CommentsViewer::menu
 * @param parent
 * @return
 */
QMenu* CommentsViewer::menu(QWidget *parent) {
	Q_ASSERT(parent);

	if (!menu_) {
		menu_ = new QMenu(tr("CommentsViewer"), parent);
		menu_->addAction(tr("&CommentsViewer"), this, SLOT(showMenu()), QKeySequence(tr("Ctrl+Alt+C")));
	}

	return menu_;
}

/**
 * @brief CommentsViewer::showMenu
 */
void CommentsViewer::showMenu() {
	if (!dialog_) {
		dialog_ = new DialogCommentsViewer(edb::v1::debugger_ui);
	}

	dialog_->show();
}

}