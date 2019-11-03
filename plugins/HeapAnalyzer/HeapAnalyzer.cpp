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
