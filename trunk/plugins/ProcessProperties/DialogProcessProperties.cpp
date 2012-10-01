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
#include "IDebuggerCore.h"
#include "Debugger.h"
#include "MemoryRegions.h"
#include <QStringList>
#include <QDebug>
#include <QDesktopServices>
#include <QFileInfo>
#include <QDir>
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
// Name: showEvent(QShowEvent *)
// Desc:
//------------------------------------------------------------------------------
void DialogProcessProperties::showEvent(QShowEvent *) {
	updateGeneralPage();
	updateMemoryPage();
	updateModulePage();
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
