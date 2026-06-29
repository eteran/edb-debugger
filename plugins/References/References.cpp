/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "References.h"
#include "DialogReferences.h"
#include "edb.h"
#include <QMenu>

namespace ReferencesPlugin {

/**
 * @brief Constructs a References object with the specified parent QObject.
 *
 * @param parent The parent QObject for this plugin.
 */
References::References(QObject *parent)
	: QObject(parent) {
}

/**
 * @brief Destroys the References object and frees the references dialog.
 */
References::~References() {
	delete dialog_;
}

/**
 * @brief Returns the menu for the References plugin.
 *
 * @param parent The parent widget for the menu.
 * @return The menu for the References plugin.
 */
QMenu *References::menu(QWidget *parent) {

	Q_ASSERT(parent);

	if (!menu_) {
		menu_ = new QMenu(tr("Reference Searcher"), parent);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
		menu_->addAction(tr("&Reference Search"), QKeySequence(tr("Ctrl+R")), this, &References::showMenu);
#else
		menu_->addAction(tr("&Reference Search"), this, &References::showMenu, QKeySequence(tr("Ctrl+R")));
#endif
	}

	return menu_;
}

/**
 * @brief Shows the Reference Search menu.
 */
void References::showMenu() {

	if (!dialog_) {
		dialog_ = new DialogReferences(edb::v1::debugger_ui);
	}

	dialog_->show();
}

}
