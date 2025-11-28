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
