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

#include "BreakpointManager.h"
#include "DialogBreakpoints.h"
#include "edb.h"
#include <QMenu>
#include <QKeySequence>

namespace BreakpointManager {

//------------------------------------------------------------------------------
// Name: BreakpointManager
// Desc:
//------------------------------------------------------------------------------
BreakpointManager::BreakpointManager() : menu_(0), dialog_(0) {
}

//------------------------------------------------------------------------------
// Name: ~BreakpointManager
// Desc:
//------------------------------------------------------------------------------
BreakpointManager::~BreakpointManager() {
	delete dialog_;
}

//------------------------------------------------------------------------------
// Name: menu
// Desc:
//------------------------------------------------------------------------------
QMenu *BreakpointManager::menu(QWidget *parent) {

	Q_ASSERT(parent);

	if(!menu_) {
		menu_ = new QMenu(tr("BreakpointManager"), parent);
		menu_->addAction(tr("&Breakpoints"), this, SLOT(show_menu()), QKeySequence(tr("Ctrl+B")));
	}

	return menu_;
}

//------------------------------------------------------------------------------
// Name: show_menu
// Desc:
//------------------------------------------------------------------------------
void BreakpointManager::show_menu() {

	if(!dialog_) {
		dialog_ = new DialogBreakpoints(edb::v1::debugger_ui);
	}

	dialog_->show();
}

#if QT_VERSION < 0x050000
Q_EXPORT_PLUGIN2(BreakpointManager, BreakpointManager)
#endif

}
