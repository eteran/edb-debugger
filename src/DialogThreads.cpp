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
	
	if(IProcess *process = edb::v1::debugger_core->process()) {
		IThread::pointer current = process->current_thread();
		
		for(IThread::pointer thread : process->threads()) {

			if(thread == current) {
				threads_model_->addThread(thread, true);
			} else {
				threads_model_->addThread(thread, false);
			}		
		}
	}

	ui->thread_table->horizontalHeader()->resizeSections(QHeaderView::Stretch);
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
