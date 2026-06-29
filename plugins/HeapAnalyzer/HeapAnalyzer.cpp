/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "HeapAnalyzer.h"
#include "DialogHeap.h"
#include "edb.h"
#include <QMenu>

namespace HeapAnalyzerPlugin {

/**
 * @brief Constructs a HeapAnalyzer object with the specified parent QObject.
 *
 * @param parent The parent QObject for this plugin.
 */
HeapAnalyzer::HeapAnalyzer(QObject *parent)
	: QObject(parent) {
}

/**
 * @brief Destructs the HeapAnalyzer object.
 */
HeapAnalyzer::~HeapAnalyzer() {
	delete dialog_;
}

/**
 * @brief Returns the menu for the HeapAnalyzer plugin.
 *
 * @param parent The parent widget for the menu.
 * @return The menu for the HeapAnalyzer plugin.
 */
QMenu *HeapAnalyzer::menu(QWidget *parent) {

	Q_ASSERT(parent);

	if (!menu_) {
		menu_ = new QMenu(tr("HeapAnalyzer"), parent);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
		menu_->addAction(tr("&Heap Analyzer"), QKeySequence(tr("Ctrl+H")), this, &HeapAnalyzer::showMenu);
#else
		menu_->addAction(tr("&Heap Analyzer"), this, &HeapAnalyzer::showMenu, QKeySequence(tr("Ctrl+H")));
#endif
	}

	return menu_;
}

/**
 * @brief Shows the Heap Analyzer menu.
 */
void HeapAnalyzer::showMenu() {

	if (!dialog_) {
		dialog_ = new DialogHeap(edb::v1::debugger_ui);
	}

	dialog_->show();
}

}
