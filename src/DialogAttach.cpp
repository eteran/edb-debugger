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
#include "IProcess.h"
#include "ProcessModel.h"
#include "edb.h"

#include <QMap>
#include <QHeaderView>
#include <QSortFilterProxyModel>

#ifdef Q_OS_WIN32
namespace {

int getuid() {
	return 0;
}

}
#else
#include <unistd.h>
#endif

namespace {

//------------------------------------------------------------------------------
// Name: isNumeric
// Desc: returns true if the string only contains decimal digits
//------------------------------------------------------------------------------
bool isNumeric(const QString &s) {
	for(QChar ch: s) {
		if(!ch.isDigit()) {
			return false;
		}
	}

	return true;
}

}

//------------------------------------------------------------------------------
// Name: DialogAttach
// Desc: constructor
//------------------------------------------------------------------------------
DialogAttach::DialogAttach(QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f) {
	ui.setupUi(this);

	process_model_ = new ProcessModel(this);

	process_name_filter_ = new QSortFilterProxyModel(this);
	process_name_filter_->setSourceModel(process_model_);
	process_name_filter_->setFilterCaseSensitivity(Qt::CaseInsensitive);
	process_name_filter_->setFilterKeyColumn(2);

	process_pid_filter_ = new QSortFilterProxyModel(this);
	process_pid_filter_->setSourceModel(process_name_filter_);
	process_pid_filter_->setFilterCaseSensitivity(Qt::CaseInsensitive);
	process_pid_filter_->setFilterKeyColumn(0);

	ui.processes_table->setModel(process_pid_filter_);
}

//------------------------------------------------------------------------------
// Name: on_filter_textChanged
// Desc:
//------------------------------------------------------------------------------
void DialogAttach::on_filter_textChanged(const QString &filter) {

	if(isNumeric(filter)) {
		process_pid_filter_->setFilterFixedString(filter);
		process_name_filter_->setFilterFixedString(QString());
	} else {
		process_name_filter_->setFilterFixedString(filter);
		process_pid_filter_->setFilterFixedString(QString());
	}
}

//------------------------------------------------------------------------------
// Name: update_list
// Desc:
//------------------------------------------------------------------------------
void DialogAttach::update_list() {

	if(isHidden()) {
		updateTimer.stop();
		return;
	}

	const auto selectedPid = selected_pid();

	process_model_->clear();

	if(edb::v1::debugger_core) {
		QMap<edb::pid_t, std::shared_ptr<IProcess>> procs = edb::v1::debugger_core->enumerate_processes();

		const edb::uid_t user_id = getuid();
		const bool filterUID = ui.filter_uid->isChecked();

		for(const std::shared_ptr<IProcess> &process: procs) {
			if(!filterUID || process->uid() == user_id) {
				process_model_->addProcess(process);
			}
		}
	}

	if(selectedPid) {
		const auto pid=selectedPid.value();
		const auto*const model=ui.processes_table->model();
		for(int row = 0; row<model->rowCount(); ++row) {
			if(static_cast<edb::pid_t>(model->index(row,0).data().toUInt()) == pid) {
				ui.processes_table->selectRow(row);
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: showEvent
// Desc:
//------------------------------------------------------------------------------
void DialogAttach::showEvent(QShowEvent *event) {
	Q_UNUSED(event)
	update_list();
	connect(&updateTimer, &QTimer::timeout, this, &DialogAttach::update_list);
	updateTimer.start(1000);
}

//------------------------------------------------------------------------------
// Name: on_filter_uid_clicked
// Desc:
//------------------------------------------------------------------------------
void DialogAttach::on_filter_uid_clicked(bool checked) {
	Q_UNUSED(checked)
	update_list();
}

//------------------------------------------------------------------------------
// Name: on_processes_table_doubleClicked
// Desc:
//------------------------------------------------------------------------------
void DialogAttach::on_processes_table_doubleClicked(const QModelIndex&) {
	if(selected_pid()) {
		accept();
	}
}

//------------------------------------------------------------------------------
// Name: selected_pid
// Desc:
//------------------------------------------------------------------------------
Result<edb::pid_t, QString> DialogAttach::selected_pid() const {

	const QItemSelectionModel *const selModel = ui.processes_table->selectionModel();
	const QModelIndexList sel = selModel->selectedRows();

	if(sel.size() == 1) {
		const QModelIndex index = process_name_filter_->mapToSource(process_pid_filter_->mapToSource(sel[0]));
		return process_model_->data(index, Qt::UserRole).toUInt();
	}

	return make_unexpected(tr("No Selection"));
}
