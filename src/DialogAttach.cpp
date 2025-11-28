/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DialogAttach.h"
#include "IDebugger.h"
#include "IProcess.h"
#include "ProcessModel.h"
#include "edb.h"
#include "util/String.h"

#include <QHeaderView>
#include <QMap>
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

//------------------------------------------------------------------------------
// Name: DialogAttach
// Desc: constructor
//------------------------------------------------------------------------------
DialogAttach::DialogAttach(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {

	ui.setupUi(this);

	processModel_ = new ProcessModel(this);

	processNameFilter_ = new QSortFilterProxyModel(this);
	processNameFilter_->setSourceModel(processModel_);
	processNameFilter_->setFilterCaseSensitivity(Qt::CaseInsensitive);
	processNameFilter_->setFilterKeyColumn(2);

	processPidFilter_ = new QSortFilterProxyModel(this);
	processPidFilter_->setSourceModel(processNameFilter_);
	processPidFilter_->setFilterCaseSensitivity(Qt::CaseInsensitive);
	processPidFilter_->setFilterKeyColumn(0);

	ui.processes_table->setModel(processPidFilter_);
}

//------------------------------------------------------------------------------
// Name: on_filter_textChanged
// Desc:
//------------------------------------------------------------------------------
void DialogAttach::on_filter_textChanged(const QString &filter) {

	if (util::is_numeric(filter)) {
		processPidFilter_->setFilterFixedString(filter);
		processNameFilter_->setFilterFixedString(QString());
	} else {
		processNameFilter_->setFilterFixedString(filter);
		processPidFilter_->setFilterFixedString(QString());
	}
}

//------------------------------------------------------------------------------
// Name: updateList
// Desc:
//------------------------------------------------------------------------------
void DialogAttach::updateList() {

	if (isHidden()) {
		updateTimer_.stop();
		return;
	}

	const auto selected_pid = selectedPid();

	processModel_->clear();

	if (edb::v1::debugger_core) {
		QMap<edb::pid_t, std::shared_ptr<IProcess>> procs = edb::v1::debugger_core->enumerateProcesses();

		const edb::uid_t user_id = getuid();
		const bool filterUID     = ui.filter_uid->isChecked();

		for (const std::shared_ptr<IProcess> &process : procs) {
			if (!filterUID || process->uid() == user_id) {
				processModel_->addProcess(process);
			}
		}
	}

	if (selected_pid) {
		const auto pid   = selected_pid.value();
		const auto model = ui.processes_table->model();
		for (int row = 0; row < model->rowCount(); ++row) {
			if (static_cast<edb::pid_t>(model->index(row, 0).data().toUInt()) == pid) {
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
	updateList();
	connect(&updateTimer_, &QTimer::timeout, this, &DialogAttach::updateList);
	updateTimer_.start(1000);
}

//------------------------------------------------------------------------------
// Name: on_filter_uid_clicked
// Desc:
//------------------------------------------------------------------------------
void DialogAttach::on_filter_uid_clicked(bool checked) {
	Q_UNUSED(checked)
	updateList();
}

//------------------------------------------------------------------------------
// Name: on_processes_table_doubleClicked
// Desc:
//------------------------------------------------------------------------------
void DialogAttach::on_processes_table_doubleClicked(const QModelIndex &) {
	if (selectedPid()) {
		accept();
	}
}

//------------------------------------------------------------------------------
// Name: selected_pid
// Desc:
//------------------------------------------------------------------------------
Result<edb::pid_t, QString> DialogAttach::selectedPid() const {

	const QItemSelectionModel *const selModel = ui.processes_table->selectionModel();
	const QModelIndexList sel                 = selModel->selectedRows();

	if (sel.size() == 1) {
		const QModelIndex index = processNameFilter_->mapToSource(processPidFilter_->mapToSource(sel[0]));
		return processModel_->data(index, Qt::UserRole).toInt();
	}

	return make_unexpected(tr("No Selection"));
}
