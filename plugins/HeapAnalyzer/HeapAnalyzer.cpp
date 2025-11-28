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
 * @brief HeapAnalyzer::HeapAnalyzer
 * @param parent
 */
HeapAnalyzer::HeapAnalyzer(QObject *parent)
	: QObject(parent) {
}

/**
 * @brief HeapAnalyzer::~HeapAnalyzer
 */
HeapAnalyzer::~HeapAnalyzer() {
	delete dialog_;
}

/**
 * @brief HeapAnalyzer::menu
 * @param parent
 * @return
 */
QMenu *HeapAnalyzer::menu(QWidget *parent) {

	Q_ASSERT(parent);

	if (!menu_) {
		menu_ = new QMenu(tr("HeapAnalyzer"), parent);
		menu_->addAction(tr("&Heap Analyzer"), this, SLOT(showMenu()), QKeySequence(tr("Ctrl+H")));
	}

	return menu_;
}

/**
 * @brief HeapAnalyzer::showMenu
 */
void HeapAnalyzer::showMenu() {

	if (!dialog_) {
		dialog_ = new DialogHeap(edb::v1::debugger_ui);
	}

	dialog_->show();
}

}
