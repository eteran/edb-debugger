/*
Copyright (C) 2006 - 2011 Evan Teran
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

#include "DialogProcessProperties.h"
#include "Debugger.h"
#include "IDebuggerCore.h"
#include "MemoryRegions.h"
#include "Configuration.h"
#include "ISymbolManager.h"
#include "Symbol.h"
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QSortFilterProxyModel>
#include <QStringList>
#include <QStringListModel>
#include <QUrl>

#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD) || defined(Q_OS_OPENBSD)
#include <link.h>
#endif

#include "ui_dialogprocess.h"

namespace {

QString size_to_string(size_t n) {
	
	if(n < 1000) {
		return QString::number(n);
	} else if(n < 1000000) {
		return QString::number(n / 1000) + " KiB";
	} else if(n < 1000000000) {
		return QString::number(n / 1000000) + " MiB";
	} else {
		return QString::number(n / 1000000) + " GiB";
	}
}

}

//------------------------------------------------------------------------------
// Name: DialogProcessProperties(QWidget *parent)
// Desc:
//------------------------------------------------------------------------------
DialogProcessProperties::DialogProcessProperties(QWidget *parent) : QDialog(parent), ui(new Ui::DialogProcessProperties) {
	ui->setupUi(this);
	ui->tableModules->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
	ui->tableMemory->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
	
	model_        = new QStringListModel(this);
	filter_model_ = new QSortFilterProxyModel(this);
	
	/*
	filter_model_->setFilterKeyColumn(0);
	filter_model_->setSourceModel(model_);
	ui->tableEnvironment->setModel(filter_model_);
	*/
}

//------------------------------------------------------------------------------
// Name: ~DialogProcessProperties()
// Desc:
//------------------------------------------------------------------------------
DialogProcessProperties::~DialogProcessProperties() {
	delete ui;
}

//------------------------------------------------------------------------------
// Name: 
// Desc:
//------------------------------------------------------------------------------
void DialogProcessProperties::updateGeneralPage() {
	if(edb::v1::debugger_core) {
	
		const edb::pid_t pid        = edb::v1::debugger_core->pid();
		const QString exe           = edb::v1::debugger_core->process_exe(pid);
		const QString cwd           = edb::v1::debugger_core->process_cwd(pid);
		const edb::pid_t parent_pid = edb::v1::debugger_core->parent_pid(pid);
		const QString parent_exe    = edb::v1::debugger_core->process_exe(parent_pid);
	
		ui->editImage->setText(exe);
		ui->editCommand->setText(QString());
		ui->editCurrentDirectory->setText(cwd);
		ui->editStarted->setText(QString());
		if(parent_pid) {
			ui->editParent->setText(QString("%1 (%2)").arg(parent_exe).arg(parent_pid));
		} else {
			ui->editParent->setText(QString());
		}
	}
}

//------------------------------------------------------------------------------
// Name: 
// Desc:
//------------------------------------------------------------------------------
void DialogProcessProperties::updateModulePage() {

	ui->tableModules->clearContents();
	ui->tableModules->setRowCount(0);
	if(edb::v1::debugger_core) {
		const QList<Module> modules = edb::v1::loaded_libraries();
		ui->tableModules->setSortingEnabled(false);
		Q_FOREACH(const Module &m, modules) {
			const int row = ui->tableModules->rowCount();
			ui->tableModules->insertRow(row);
			ui->tableModules->setItem(row, 0, new QTableWidgetItem(edb::v1::format_pointer(m.base_address)));
			ui->tableModules->setItem(row, 1, new QTableWidgetItem(m.name));
		}
		ui->tableModules->setSortingEnabled(true);
	}
	
}

//------------------------------------------------------------------------------
// Name: 
// Desc:
//------------------------------------------------------------------------------
void DialogProcessProperties::updateMemoryPage() {

	ui->tableMemory->clearContents();
	ui->tableMemory->setRowCount(0);

	if(edb::v1::debugger_core) {
		edb::v1::memory_regions().sync();
		const QList<MemoryRegion> regions = edb::v1::memory_regions().regions();
		ui->tableMemory->setSortingEnabled(false);


		Q_FOREACH(const MemoryRegion &r, regions) {
			const int row = ui->tableMemory->rowCount();
			ui->tableMemory->insertRow(row);
			ui->tableMemory->setItem(row, 0, new QTableWidgetItem(edb::v1::format_pointer(r.start()))); // address
			ui->tableMemory->setItem(row, 1, new QTableWidgetItem(size_to_string(r.size())));           // size
			ui->tableMemory->setItem(row, 2, new QTableWidgetItem(QString("%1%2%3")                     // protection
				.arg(r.readable() ? 'r' : '-')
				.arg(r.writable() ? 'w' : '-')
				.arg(r.executable() ? 'x' : '-'))); 
			ui->tableMemory->setItem(row, 3, new QTableWidgetItem(r.name()));                           // name
		}
		ui->tableMemory->setSortingEnabled(true);
	}
}

//------------------------------------------------------------------------------
// Name: 
// Desc:
//------------------------------------------------------------------------------
void DialogProcessProperties::on_txtSearchEnvironment_textChanged(const QString &text) {
	updateEnvironmentPage(text);
}

//------------------------------------------------------------------------------
// Name: 
// Desc:
//------------------------------------------------------------------------------
void DialogProcessProperties::updateEnvironmentPage(const QString &filter) {
	// tableEnvironment
	
	ui->tableEnvironment->clearContents();
	ui->tableEnvironment->setSortingEnabled(false);
	ui->tableEnvironment->setRowCount(0);
	
	const QString lower_filter = filter.toLower();
	
#ifdef Q_OS_LINUX
	QFile proc_environ(QString("/proc/%1/environ").arg(edb::v1::debugger_core->pid()));
	if(proc_environ.open(QIODevice::ReadOnly)) {
		QByteArray env = proc_environ.readAll();
		char *p = env.data();
		char *ptr = p;
		while(ptr != p + env.size()) {
			const QString env = QString::fromUtf8(ptr);
			const QString env_name  = env.mid(0, env.indexOf("="));
			const QString env_value = env.mid(env.indexOf("=") + 1);

			if(lower_filter.isEmpty() || env_name.toLower().contains(lower_filter)) {
				const int row = ui->tableEnvironment->rowCount();
				ui->tableEnvironment->insertRow(row);
				ui->tableEnvironment->setItem(row, 0, new QTableWidgetItem(env_name));
			    ui->tableEnvironment->setItem(row, 1, new QTableWidgetItem(env_value));
			}
			
			ptr += qstrlen(ptr) + 1;
		}

	}
#endif

	ui->tableEnvironment->setSortingEnabled(true);
}

//------------------------------------------------------------------------------
// Name: showEvent(QShowEvent *)
// Desc:
//------------------------------------------------------------------------------
void DialogProcessProperties::showEvent(QShowEvent *) {
	updateGeneralPage();
	updateMemoryPage();
	updateModulePage();
	updateEnvironmentPage(ui->txtSearchEnvironment->text());
}

//------------------------------------------------------------------------------
// Name: 
// Desc:
//------------------------------------------------------------------------------
void DialogProcessProperties::on_btnParent_clicked() {

	if(edb::v1::debugger_core) {
		const edb::pid_t pid        = edb::v1::debugger_core->pid();
		const edb::pid_t parent_pid = edb::v1::debugger_core->parent_pid(pid);
		const QString parent_exe    = edb::v1::debugger_core->process_exe(parent_pid);
		QFileInfo info(parent_exe);
		QDir dir = info.absoluteDir();
		QDesktopServices::openUrl(QUrl(QString("file:///%1").arg(dir.absolutePath()), QUrl::TolerantMode));
	}
}

//------------------------------------------------------------------------------
// Name: 
// Desc:
//------------------------------------------------------------------------------
void DialogProcessProperties::on_btnImage_clicked() {
	if(edb::v1::debugger_core) {
		QFileInfo info(ui->editImage->text());
		QDir dir = info.absoluteDir();
		QDesktopServices::openUrl(QUrl(QString("file:///%1").arg(dir.absolutePath()), QUrl::TolerantMode));
	}
}

//------------------------------------------------------------------------------
// Name: 
// Desc:
//------------------------------------------------------------------------------
void DialogProcessProperties::on_btnRefreshEnvironment_clicked() {
	updateEnvironmentPage(ui->txtSearchEnvironment->text());
}
