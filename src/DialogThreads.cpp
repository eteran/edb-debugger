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

#include "DialogThreads.h"
#include "IDebugger.h"
#include "IProcess.h"
#include "IThread.h"
#include "ThreadsModel.h"
#include "edb.h"

#include <QHeaderView>
#include <QSortFilterProxyModel>

//------------------------------------------------------------------------------
// Name: DialogThreads
// Desc:
//------------------------------------------------------------------------------
DialogThreads::DialogThreads(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {

	ui.setupUi(this);

	threadsModel_  = new ThreadsModel(this);
	threadsFilter_ = new QSortFilterProxyModel(this);

	threadsFilter_->setSourceModel(threadsModel_);
	threadsFilter_->setFilterCaseSensitivity(Qt::CaseInsensitive);

	ui.thread_table->setModel(threadsFilter_);

	connect(edb::v1::debugger_ui, SIGNAL(debugEvent()), this, SLOT(updateThreads()));
	connect(edb::v1::debugger_ui, SIGNAL(detachEvent()), this, SLOT(updateThreads()));
	connect(edb::v1::debugger_ui, SIGNAL(attachEvent()), this, SLOT(updateThreads()));
}

//------------------------------------------------------------------------------
// Name: showEvent
// Desc:
//------------------------------------------------------------------------------
void DialogThreads::showEvent(QShowEvent *) {
	updateThreads();
}

//------------------------------------------------------------------------------
// Name: on_thread_table_doubleClicked
// Desc:
//------------------------------------------------------------------------------
void DialogThreads::on_thread_table_doubleClicked(const QModelIndex &index) {

	const QModelIndex internal_index = threadsFilter_->mapToSource(index);
	if (auto item = reinterpret_cast<ThreadsModel::Item *>(internal_index.internalPointer())) {
		if (std::shared_ptr<IThread> thread = item->thread) {
			if (IProcess *process = edb::v1::debugger_core->process()) {
				process->setCurrentThread(*thread);
				updateThreads();
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: updateThreads
// Desc:
//------------------------------------------------------------------------------
void DialogThreads::updateThreads() {
	threadsModel_->clear();

	if (IProcess *process = edb::v1::debugger_core->process()) {
		std::shared_ptr<IThread> current = process->currentThread();

		for (std::shared_ptr<IThread> &thread : process->threads()) {

			if (thread == current) {
				threadsModel_->addThread(thread, true);
			} else {
				threadsModel_->addThread(thread, false);
			}
		}
	}

	ui.thread_table->horizontalHeader()->resizeSections(QHeaderView::Stretch);
}
