/*
Copyright (C) 2006 - 2014 Evan Teran
                          eteran@alum.rit.edu

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
#include "IDebuggerCore.h"
#include "ThreadsModel.h"
#include "edb.h"

#include <QHeaderView>
#include <QSortFilterProxyModel>

#include "ui_DialogThreads.h"

//------------------------------------------------------------------------------
// Name: DialogThreads
// Desc:
//------------------------------------------------------------------------------
DialogThreads::DialogThreads(QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f), ui(new Ui::DialogThreads) {
	ui->setupUi(this);
#if QT_VERSION >= 0x050000
	ui->thread_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
#else
	ui->thread_table->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
#endif

	threads_model_ = new ThreadsModel(this);
	threads_filter_ = new QSortFilterProxyModel(this);
	
	threads_filter_->setSourceModel(threads_model_);
	threads_filter_->setFilterCaseSensitivity(Qt::CaseInsensitive);
	
	ui->thread_table->setModel(threads_filter_);
}

//------------------------------------------------------------------------------
// Name: ~DialogThreads
// Desc:
//------------------------------------------------------------------------------
DialogThreads::~DialogThreads() {
	delete ui;
}

//------------------------------------------------------------------------------
// Name: showEvent
// Desc:
//------------------------------------------------------------------------------
void DialogThreads::showEvent(QShowEvent *) {

	threads_model_->clear();

	QList<edb::tid_t> threads       = edb::v1::debugger_core->thread_ids();
	const edb::tid_t current_thread = edb::v1::debugger_core->active_thread();

	Q_FOREACH(edb::tid_t thread, threads) {
		if(thread == current_thread) {
			threads_model_->addThread(thread, true);
		} else {
			threads_model_->addThread(thread, false);
		}
	}

	ui->thread_table->resizeColumnsToContents();
}

//------------------------------------------------------------------------------
// Name: selected_thread
// Desc:
//------------------------------------------------------------------------------
edb::tid_t DialogThreads::selected_thread() {
	const QItemSelectionModel *const selModel = ui->thread_table->selectionModel();
	const QModelIndexList sel = selModel->selectedRows();

	if(sel.size() == 1) {
		const QModelIndex index = threads_filter_->mapToSource(sel[0]);
		return threads_model_->data(index, Qt::UserRole).toUInt();
	}
	
	return 0;
}
