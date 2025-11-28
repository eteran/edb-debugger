/*
 * Copyright (C) 2015 Armen Boursalian <aboursalian@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "Backtrace.h"
#include "DialogBacktrace.h"
#include "edb.h"

#include <QKeySequence>
#include <QMenu>

namespace BacktracePlugin {

/**
 * @brief Backtrace::Backtrace
 * @param parent
 */
Backtrace::Backtrace(QObject *parent)
	: QObject(parent) {
}

/**
 * @brief Backtrace::~Backtrace
 */
Backtrace::~Backtrace() {
	delete dialog_;
}

/**
 * @brief Backtrace::menu
 * @param parent
 * @return
 */
QMenu *Backtrace::menu(QWidget *parent) {
	Q_ASSERT(parent);

	if (!menu_) {

		// So it will appear as Plugins > Call Stack > Backtrace in the menu.
		menu_ = new QMenu(tr("Call Stack"), parent);

		// Ctrl + K shortcut, reminiscent of OllyDbg
		menu_->addAction(tr("Backtrace"), this, SLOT(showMenu()), QKeySequence(tr("Ctrl+K")));
	}

	return menu_;
}

/**
 * @brief Backtrace::showMenu
 */
void Backtrace::showMenu() {
	if (!dialog_) {
		dialog_ = new DialogBacktrace(edb::v1::debugger_ui);
	}
	dialog_->show();
}

}
