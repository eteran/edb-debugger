/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "FunctionFinder.h"
#include "DialogFunctions.h"
#include "edb.h"
#include <QMenu>

namespace FunctionFinderPlugin {

/**
 * @brief Constructs a FunctionFinder object with the specified parent widget.
 *
 * @param parent The parent widget for this object.
 */
FunctionFinder::FunctionFinder(QObject *parent)
	: QObject(parent) {
}

/**
 * @brief Destroys the FunctionFinder object and frees the dialog.
 */
FunctionFinder::~FunctionFinder() {
	delete dialog_;
}

/**
 * @brief Creates and returns the menu for the FunctionFinder plugin, adding a "Function Finder" action to it. The menu is created on the first call.
 *
 * @param parent The parent widget for this menu.
 * @return A pointer to the created menu.
 */
QMenu *FunctionFinder::menu(QWidget *parent) {

	Q_ASSERT(parent);

	if (!menu_) {
		menu_ = new QMenu(tr("FunctionFinder"), parent);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
		menu_->addAction(tr("&Function Finder"), QKeySequence(tr("Ctrl+Shift+F")), this, &FunctionFinder::showMenu);
#else
		menu_->addAction(tr("&Function Finder"), this, &FunctionFinder::showMenu, QKeySequence(tr("Ctrl+Shift+F")));
#endif
	}

	return menu_;
}

/**
 * @brief Shows the Function Finder dialog, creating it if it does not already exist.
 */
void FunctionFinder::showMenu() {

	if (!dialog_) {
		dialog_ = new DialogFunctions(edb::v1::debugger_ui);
	}

	dialog_->show();
}

}
