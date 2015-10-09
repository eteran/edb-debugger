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

#include "Assembler.h"
#include "edb.h"
#include "DialogAssembler.h"
#include "MemoryRegions.h"
#include "OptionsPage.h"

#include <QMenu>
#include <QList>
#include <QAction>

namespace Assembler {

//------------------------------------------------------------------------------
// Name: Assembler
// Desc:
//------------------------------------------------------------------------------
Assembler::Assembler() : dialog_(0) {
}

//------------------------------------------------------------------------------
// Name: ~Assembler
// Desc:
//------------------------------------------------------------------------------
Assembler::~Assembler() {
	delete dialog_;
}

//------------------------------------------------------------------------------
// Name: cpu_context_menu
// Desc:
//------------------------------------------------------------------------------
QList<QAction *> Assembler::cpu_context_menu() {

	QList<QAction *> ret;

	auto action_assemble = new QAction(tr("Assemble"), this);
	action_assemble->setShortcut(QKeySequence(tr("Space")));


	connect(action_assemble, SIGNAL(triggered()), this, SLOT(show_dialog()));
	ret << action_assemble;

	return ret;
}

//------------------------------------------------------------------------------
// Name: menu
// Desc:
//------------------------------------------------------------------------------
QMenu *Assembler::menu(QWidget *parent) {
	Q_UNUSED(parent);
	return 0;
}

//------------------------------------------------------------------------------
// Name: show_dialog
// Desc:
//------------------------------------------------------------------------------
void Assembler::show_dialog() {

	if(!dialog_) {
		dialog_ = new DialogAssembler(edb::v1::debugger_ui);
	}
	
	const edb::address_t address = edb::v1::cpu_selected_address();
	if(IRegion::pointer region = edb::v1::memory_regions().find_region(address)) {
		if(auto d = qobject_cast<DialogAssembler *>(dialog_)) {
			d->set_address(address);
		}
		dialog_->show();
	}
}

//------------------------------------------------------------------------------
// Name: options_page
// Desc:
//------------------------------------------------------------------------------
QWidget *Assembler::options_page() {
	return new OptionsPage;
}

#if QT_VERSION < 0x050000
Q_EXPORT_PLUGIN2(Assembler, Assembler)
#endif

}
