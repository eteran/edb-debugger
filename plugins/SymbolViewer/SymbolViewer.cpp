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
