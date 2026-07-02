/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
 * @brief Constructs the Assembler plugin object.
 *
 * @param parent
 */
Assembler::Assembler(QObject *parent)
	: QObject(parent) {
}

/**
 * @brief Destroys the Assembler plugin and frees the assembler dialog.
 */
Assembler::~Assembler() {
	delete dialog_;
}

/**
 * @brief Returns the CPU context menu actions contributed by the Assembler plugin.
 *
 * @return
 */
QList<QAction *> Assembler::cpuContextMenu() {

	auto action_assemble = new QAction(tr("&Assemble..."), this);
	action_assemble->setShortcut(QKeySequence(tr("Space")));

	connect(action_assemble, &QAction::triggered, this, &Assembler::showDialog);

	return {action_assemble};
}

/**
 * @brief Returns nullptr, as the Assembler plugin contributes no top-level menu.
 *
 * @param parent
 * @return
 */
QMenu *Assembler::menu(QWidget *parent) {
	Q_UNUSED(parent)
	return nullptr;
}

/**
 * @brief Opens the assembler dialog at the currently selected CPU address.
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
 * @brief Returns the plugin's configuration options widget.
 *
 * @return
 */
QWidget *Assembler::optionsPage() {
	return new OptionsPage;
}

}
