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

#include "DialogAttach.h"
#include "IDebugger.h"
#include "ProcessModel.h"
#include "Types.h"
#include "edb.h"

#include <QMap>
#include <QHeaderView>
#include <QSortFilterProxyModel>

#include "ui_DialogAttach.h"

#ifdef Q_OS_WIN32
namespace {

int getuid() {
	return 0;
}

}
#else
#include <unistd.h>
#endif

//------------------------------------------------------------------------------
// Name: DialogAttach
// Desc: constructor
//------------------------------------------------------------------------------
DialogAttach::DialogAttach(QWidget *parent) : QDialog(parent), ui(new Ui::DialogAttach) {
	ui->setupUi(this);

	process_model_ = new ProcessModel(this);
	process_filter_ = new QSortFilterProxyModel(this);

	process_filter_->setSourceModel(process_model_);
	process_filter_->setFilterCaseSensitivity(Qt::CaseInsensitive);
	process_filter_->setFilterKeyColumn(2);

	ui->processes_table->setModel(process_filter_);

	connect(ui->filter, SIGNAL(textChanged(const QString &)), process_filter_, SLOT(setFilterFixedString(const QString &)));
}

//------------------------------------------------------------------------------
// Name: ~DialogAttach
// Desc:
//------------------------------------------------------------------------------
DialogAttach::~DialogAttach() {
	delete ui;
}

//------------------------------------------------------------------------------
// Name: update_list
// Desc:
//------------------------------------------------------------------------------
void DialogAttach::update_list() {

	process_model_->clear();

	if(edb::v1::debugger_core) {
		QMap<edb::pid_t, IProcess::pointer> procs = edb::v1::debugger_core->enumerate_processes();

		const edb::uid_t user_id = getuid();
		const bool filterUID = ui->filter_uid->isChecked();

		for(const IProcess::pointer &process: procs) {
			if(!filterUID || process->uid() == user_id) {
				process_model_->addProcess(process);
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: showEvent
// Desc:
//------------------------------------------------------------------------------
void DialogAttach::showEvent(QShowEvent *event) {
	Q_UNUSED(event);
	update_list();
}

//------------------------------------------------------------------------------
// Name: on_filter_uid_clicked
// Desc:
//------------------------------------------------------------------------------
void DialogAttach::on_filter_uid_clicked(bool checked) {
	Q_UNUSED(checked);
	update_list();
}

//------------------------------------------------------------------------------
// Name: selected_pid
// Desc:
//------------------------------------------------------------------------------
edb::pid_t DialogAttach::selected_pid(bool *ok) const {

	Q_ASSERT(ok);

	const QItemSelectionModel *const selModel = ui->processes_table->selectionModel();
	const QModelIndexList sel = selModel->selectedRows();

	if(sel.size() == 1) {
		const QModelIndex index = process_filter_->mapToSource(sel[0]);
		*ok = true;
		return process_model_->data(index, Qt::UserRole).toUInt();
	}

	*ok = false;
	return 0;
}
