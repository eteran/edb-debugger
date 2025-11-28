/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
