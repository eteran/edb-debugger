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
#include "DialogAssembler.h"
#include "MemoryRegions.h"
#include "OptionsPage.h"
#include "edb.h"

#include <QAction>
#include <QList>
#include <QMenu>

namespace AssemblerPlugin {

/**
 * @brief Assembler::Assembler
 * @param parent
 */
Assembler::Assembler(QObject *parent)
	: QObject(parent) {
}

/**
 * @brief Assembler::~Assembler
 */
Assembler::~Assembler() {
	delete dialog_;
}

/**
 * @brief Assembler::cpuContextMenu
 * @return
 */
QList<QAction *> Assembler::cpuContextMenu() {

	QList<QAction *> ret;

	auto action_assemble = new QAction(tr("&Assemble..."), this);
	action_assemble->setShortcut(QKeySequence(tr("Space")));

	connect(action_assemble, &QAction::triggered, this, &Assembler::showDialog);
	ret << action_assemble;

	return ret;
}

/**
 * @brief Assembler::menu
 * @param parent
 * @return
 */
QMenu *Assembler::menu(QWidget *parent) {
	Q_UNUSED(parent)
	return nullptr;
}

/**
 * @brief Assembler::showDialog
 */
void Assembler::showDialog() {

	if (!dialog_) {
		dialog_ = new DialogAssembler(edb::v1::debugger_ui);
	}

	const edb::address_t address = edb::v1::cpu_selected_address();
	if (std::shared_ptr<IRegion> region = edb::v1::memory_regions().findRegion(address)) {
		if (auto d = qobject_cast<DialogAssembler *>(dialog_)) {
			d->setAddress(address);
		}
		dialog_->show();
	}
}

/**
 * @brief Assembler::optionsPage
 * @return
 */
QWidget *Assembler::optionsPage() {
	return new OptionsPage;
}

}
