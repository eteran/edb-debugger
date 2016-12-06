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

#include "DialogProcessProperties.h"
#include "edb.h"
#include "IDebugger.h"
#include "MemoryRegions.h"
#include "Configuration.h"
#include "ISymbolManager.h"
#include "DialogStrings.h"
#include "Symbol.h"

#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QHostAddress>
#include <QStringList>
#include <QStringListModel>
#include <QUrl>

#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD) || defined(Q_OS_OPENBSD)
#include <link.h>
#include <arpa/inet.h>
#endif

#include "ui_DialogProcessProperties.h"

namespace ProcessProperties {

namespace {

QString size_to_string(size_t n) {

	static constexpr size_t KiB = 1024;
	static constexpr size_t MiB = KiB * 1024;
	static constexpr size_t GiB = MiB * 1024;
	
	if(n < KiB) {
		return QString::number(n);
	} else if(n < MiB) {
		return QString::number(n / KiB) + " KiB";
	} else if(n < GiB) {
		return QString::number(n / MiB) + " MiB";
	} else {
		return QString::number(n / GiB) + " GiB";
	}
}

#if defined(Q_OS_LINUX)
//------------------------------------------------------------------------------
// Name: tcp_socket_prcoessor
// Desc:
//------------------------------------------------------------------------------
bool tcp_socket_prcoessor(QString *symlink, int sock, const QStringList &lst) {

	Q_ASSERT(symlink);

	if(lst.size() >= 13) {

		bool ok;
		const quint32 local_address = ntohl(lst[1].toUInt(&ok, 16));
		if(ok) {
			const quint16 local_port = lst[2].toUInt(&ok, 16);
			if(ok) {
				const quint32 remote_address = ntohl(lst[3].toUInt(&ok, 16));
				if(ok) {
					const quint16 remote_port = lst[4].toUInt(&ok, 16);
					if(ok) {
						const quint8 state = lst[5].toUInt(&ok, 16);
						Q_UNUSED(state);
						if(ok) {
							const int inode = lst[13].toUInt(&ok, 10);
							if(ok) {
								if(inode == sock) {
									*symlink = QString("TCP: %1:%2 -> %3:%4")
										.arg(QHostAddress(local_address).toString())
										.arg(local_port)
										.arg(QHostAddress(remote_address).toString())
										.arg(remote_port);
										return true;
								}
							}
						}
					}
				}
			}
		}
	}
	return false;
}

//------------------------------------------------------------------------------
// Name: udp_socket_processor
// Desc:
//------------------------------------------------------------------------------
bool udp_socket_processor(QString *symlink, int sock, const QStringList &lst) {

	Q_ASSERT(symlink);

	if(lst.size() >= 13) {

		bool ok;
		const quint32 local_address = ntohl(lst[1].toUInt(&ok, 16));
		if(ok) {
			const quint16 local_port = lst[2].toUInt(&ok, 16);
			if(ok) {
				const quint32 remote_address = ntohl(lst[3].toUInt(&ok, 16));
				if(ok) {
					const quint16 remote_port = lst[4].toUInt(&ok, 16);
					if(ok) {
						const quint8 state = lst[5].toUInt(&ok, 16);
						Q_UNUSED(state);
						if(ok) {
							const int inode = lst[13].toUInt(&ok, 10);
							if(ok) {
								if(inode == sock) {
									*symlink = QString("UDP: %1:%2 -> %3:%4")
										.arg(QHostAddress(local_address).toString())
										.arg(local_port)
										.arg(QHostAddress(remote_address).toString())
										.arg(remote_port);
										return true;
								}
							}
						}
					}
				}
			}
		}
	}
	return false;
}

//------------------------------------------------------------------------------
// Name: unix_socket_processor
// Desc:
//------------------------------------------------------------------------------
bool unix_socket_processor(QString *symlink, int sock, const QStringList &lst) {

	Q_ASSERT(symlink);

	if(lst.size() >= 6) {
		bool ok;
		const int inode = lst[6].toUInt(&ok, 10);
		if(ok) {
			if(inode == sock) {
				*symlink = QString("UNIX [%1]").arg(lst[0]);
				return true;
			}
		}
	}
	return false;
}

//------------------------------------------------------------------------------
// Name: process_socket_file
// Desc:
//------------------------------------------------------------------------------
template <class F>
QString process_socket_file(const QString &filename, QString *symlink, int sock, F fp) {

	Q_ASSERT(symlink);

	QFile net(filename);
	net.open(QIODevice::ReadOnly | QIODevice::Text);
	if(net.isOpen()) {
		QTextStream in(&net);
		QString line;

		// ditch first line, it is just table headings
		in.readLine();

		// read in the first line we care about
		line = in.readLine();

		// a null string means end of file (but not an empty string!)
		while(!line.isNull()) {

			QString lline(line);
			const QStringList lst = lline.replace(":", " ").split(" ", QString::SkipEmptyParts);

			if(fp(symlink, sock, lst)) {
				break;
			}

			line = in.readLine();
		}
	}
	return *symlink;
}

//------------------------------------------------------------------------------
// Name: process_socket_tcp
// Desc:
//------------------------------------------------------------------------------
QString process_socket_tcp(QString *symlink) {

	Q_ASSERT(symlink);

	const QString socket_info(symlink->mid(symlink->indexOf("socket:[")));
	const int socket_number = socket_info.mid(8).remove("]").toUInt();

	return process_socket_file("/proc/net/tcp", symlink, socket_number, tcp_socket_prcoessor);
}

//------------------------------------------------------------------------------
// Name: process_socket_unix
// Desc:
//------------------------------------------------------------------------------
QString process_socket_unix(QString *symlink) {

	Q_ASSERT(symlink);

	const QString socket_info(symlink->mid(symlink->indexOf("socket:[")));
	const int socket_number = socket_info.mid(8).remove("]").toUInt();

	return process_socket_file("/proc/net/unix", symlink, socket_number, unix_socket_processor);
}

//------------------------------------------------------------------------------
// Name: process_socket_udp
// Desc:
//------------------------------------------------------------------------------
QString process_socket_udp(QString *symlink) {

	Q_ASSERT(symlink);

	const QString socket_info(symlink->mid(symlink->indexOf("socket:[")));
	const int socket_number = socket_info.mid(8).remove("]").toUInt();

	return process_socket_file("/proc/net/udp", symlink, socket_number, udp_socket_processor);
}

//------------------------------------------------------------------------------
// Name: file_type
// Desc:
//------------------------------------------------------------------------------
QString file_type(const QString &filename) {
	const QFileInfo info(filename);
	const QString basename(info.completeBaseName());

	if(basename.startsWith("socket:")) {
		return QT_TRANSLATE_NOOP("DialogProcessProperties", "Socket");
	}

	if(basename.startsWith("pipe:")) {
		return QT_TRANSLATE_NOOP("DialogProcessProperties", "Pipe");
	}

	return QT_TRANSLATE_NOOP("DialogProcessProperties", "File");
}
#endif

}




//------------------------------------------------------------------------------
// Name: DialogProcessProperties
// Desc:
//------------------------------------------------------------------------------
DialogProcessProperties::DialogProcessProperties(QWidget *parent) : QDialog(parent), ui(new Ui::DialogProcessProperties) {
	ui->setupUi(this);
#if QT_VERSION >= 0x050000
	ui->tableModules->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
	ui->tableMemory->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
	ui->threadTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
#else
	ui->tableModules->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
	ui->tableMemory->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
	ui->threadTable->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
#endif

	threads_model_ = new ThreadsModel(this);
	threads_filter_ = new QSortFilterProxyModel(this);
	
	threads_filter_->setSourceModel(threads_model_);
	threads_filter_->setFilterCaseSensitivity(Qt::CaseInsensitive);
	
	ui->threadTable->setModel(threads_filter_);
}

//------------------------------------------------------------------------------
// Name: ~DialogProcessProperties
// Desc:
//------------------------------------------------------------------------------
DialogProcessProperties::~DialogProcessProperties() {
	delete ui;
}

//------------------------------------------------------------------------------
// Name: updateGeneralPage
// Desc:
//------------------------------------------------------------------------------
void DialogProcessProperties::updateGeneralPage() {
	if(edb::v1::debugger_core) {
	    if(IProcess *process = edb::v1::debugger_core->process()) {
	        const QString exe            = process->executable();
	        const QString cwd            = process->current_working_directory();
			
			std::shared_ptr<IProcess> parent = process->parent();
	        const edb::pid_t parent_pid  = parent ? parent->pid() : 0;
	        const QString parent_exe     = parent ? parent->executable() : QString();
			
	        const QList<QByteArray> args = process->arguments();

			ui->editImage->setText(exe);
			ui->editCommand->setText(QString());
			ui->editCurrentDirectory->setText(cwd);
			ui->editStarted->setText(process->start_time().toString("yyyy-MM-dd hh:mm:ss.z"));
			if(parent_pid) {
				ui->editParent->setText(QString("%1 (%2)").arg(parent_exe).arg(parent_pid));
			} else {
				ui->editParent->setText(QString());
			}
		} else {
			ui->editImage->setText(QString());
			ui->editCommand->setText(QString());
			ui->editCurrentDirectory->setText(QString());
			ui->editStarted->setText(QString());
			ui->editParent->setText(QString());
		}
	}
}

//------------------------------------------------------------------------------
// Name: updateModulePage
// Desc:
//------------------------------------------------------------------------------
void DialogProcessProperties::updateModulePage() {

	ui->tableModules->clearContents();
	ui->tableModules->setRowCount(0);
	if(edb::v1::debugger_core) {
		if(IProcess *process = edb::v1::debugger_core->process()) {
			const QList<Module> modules = process->loaded_modules();
			ui->tableModules->setSortingEnabled(false);
			for(const Module &m: modules) {
				const int row = ui->tableModules->rowCount();
				ui->tableModules->insertRow(row);
				ui->tableModules->setItem(row, 0, new QTableWidgetItem(edb::v1::format_pointer(m.base_address)));
				ui->tableModules->setItem(row, 1, new QTableWidgetItem(m.name));
			}
			ui->tableModules->setSortingEnabled(true);
		}
	}

}

//------------------------------------------------------------------------------
// Name: updateMemoryPage
// Desc:
//------------------------------------------------------------------------------
void DialogProcessProperties::updateMemoryPage() {

	ui->tableMemory->clearContents();
	ui->tableMemory->setRowCount(0);

	if(edb::v1::debugger_core) {
		edb::v1::memory_regions().sync();
		const QList<IRegion::pointer> regions = edb::v1::memory_regions().regions();
		ui->tableMemory->setSortingEnabled(false);


		for(const IRegion::pointer &r: regions) {
			const int row = ui->tableMemory->rowCount();
			ui->tableMemory->insertRow(row);
			ui->tableMemory->setItem(row, 0, new QTableWidgetItem(edb::v1::format_pointer(r->start()))); // address
			ui->tableMemory->setItem(row, 1, new QTableWidgetItem(size_to_string(r->size())));           // size
			ui->tableMemory->setItem(row, 2, new QTableWidgetItem(QString("%1%2%3")                      // protection
				.arg(r->readable() ? 'r' : '-')
				.arg(r->writable() ? 'w' : '-')
				.arg(r->executable() ? 'x' : '-')));
			ui->tableMemory->setItem(row, 3, new QTableWidgetItem(r->name()));                           // name
		}
		ui->tableMemory->setSortingEnabled(true);
	}
}

//------------------------------------------------------------------------------
// Name: on_txtSearchEnvironment_textChanged
// Desc:
//------------------------------------------------------------------------------
void DialogProcessProperties::on_txtSearchEnvironment_textChanged(const QString &text) {
	updateEnvironmentPage(text);
}

//------------------------------------------------------------------------------
// Name: updateEnvironmentPage
// Desc:
//------------------------------------------------------------------------------
void DialogProcessProperties::updateEnvironmentPage(const QString &filter) {
	// tableEnvironment

	ui->tableEnvironment->clearContents();
	ui->tableEnvironment->setSortingEnabled(false);
	ui->tableEnvironment->setRowCount(0);

	const QString lower_filter = filter.toLower();

#ifdef Q_OS_LINUX
	if(IProcess *process = edb::v1::debugger_core->process()) {
		QFile proc_environ(QString("/proc/%1/environ").arg(process->pid()));
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
	}
#endif

	ui->tableEnvironment->setSortingEnabled(true);
}

//------------------------------------------------------------------------------
// Name: updateHandles
// Desc:
//------------------------------------------------------------------------------
void DialogProcessProperties::updateHandles() {

	ui->tableHandles->setSortingEnabled(false);
	ui->tableHandles->setRowCount(0);

#ifdef Q_OS_LINUX
	if(IProcess *process = edb::v1::debugger_core->process()) {
		QDir dir(QString("/proc/%1/fd/").arg(process->pid()));
		const QFileInfoList entries = dir.entryInfoList(QStringList() << "[0-9]*");
		for(const QFileInfo &info: entries) {
			if(info.isSymLink()) {
				QString symlink(info.symLinkTarget());
				const QString type(file_type(symlink));

				if(type == tr("Socket")) {
					symlink = process_socket_tcp(&symlink);
					symlink = process_socket_udp(&symlink);
					symlink = process_socket_unix(&symlink);
				}

				if(type == tr("Pipe")) {
					symlink = tr("FIFO");
				}

				const int row = ui->tableHandles->rowCount();
				ui->tableHandles->insertRow(row);


				auto itemFD = new QTableWidgetItem;
				itemFD->setData(Qt::DisplayRole, info.fileName().toUInt());

				ui->tableHandles->setItem(row, 0, new QTableWidgetItem(type));
				ui->tableHandles->setItem(row, 1, itemFD);
				ui->tableHandles->setItem(row, 2, new QTableWidgetItem(symlink));
			}
		}
	}
#endif

	ui->tableHandles->setSortingEnabled(true);

}

//------------------------------------------------------------------------------
// Name: showEvent
// Desc:
//------------------------------------------------------------------------------
void DialogProcessProperties::showEvent(QShowEvent *) {
	updateGeneralPage();
	updateMemoryPage();
	updateModulePage();
	updateHandles();
	updateThreads();
	updateEnvironmentPage(ui->txtSearchEnvironment->text());
}

//------------------------------------------------------------------------------
// Name: on_btnParent_clicked
// Desc:
//------------------------------------------------------------------------------
void DialogProcessProperties::on_btnParent_clicked() {

	if(edb::v1::debugger_core) {
		if(IProcess *process = edb::v1::debugger_core->process()) {
		
			std::shared_ptr<IProcess> parent = process->parent();
	        const QString parent_exe     = parent ? parent->executable() : QString();					

			QFileInfo info(parent_exe);
			QDir dir = info.absoluteDir();
			QDesktopServices::openUrl(QUrl(tr("file:///%1").arg(dir.absolutePath()), QUrl::TolerantMode));
		}
	}
}

//------------------------------------------------------------------------------
// Name: on_btnImage_clicked
// Desc:
//------------------------------------------------------------------------------
void DialogProcessProperties::on_btnImage_clicked() {
	if(edb::v1::debugger_core) {
		QFileInfo info(ui->editImage->text());
		QDir dir = info.absoluteDir();
		QDesktopServices::openUrl(QUrl(tr("file:///%1").arg(dir.absolutePath()), QUrl::TolerantMode));
	}
}

//------------------------------------------------------------------------------
// Name: on_btnRefreshEnvironment_clicked
// Desc:
//------------------------------------------------------------------------------
void DialogProcessProperties::on_btnRefreshEnvironment_clicked() {
	updateEnvironmentPage(ui->txtSearchEnvironment->text());
}

//------------------------------------------------------------------------------
// Name: on_btnRefreshHandles_clicked
// Desc:
//------------------------------------------------------------------------------
void DialogProcessProperties::on_btnRefreshHandles_clicked() {
	updateHandles();
}

//------------------------------------------------------------------------------
// Name: on_btnStrings_clicked
// Desc:
//------------------------------------------------------------------------------
void DialogProcessProperties::on_btnStrings_clicked() {

	static auto dialog = new DialogStrings(edb::v1::debugger_ui);
	dialog->show();
}

//------------------------------------------------------------------------------
// Name: on_btnRefreshMemory_clicked
// Desc:
//------------------------------------------------------------------------------
void DialogProcessProperties::on_btnRefreshMemory_clicked() {
	updateMemoryPage();
}

//------------------------------------------------------------------------------
// Name: on_btnRefreshThreads_clicked
// Desc:
//------------------------------------------------------------------------------
void DialogProcessProperties::on_btnRefreshThreads_clicked() {
	updateThreads();
}

//------------------------------------------------------------------------------
// Name: updateThreads
// Desc:
//------------------------------------------------------------------------------
void DialogProcessProperties::updateThreads() {
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
}

}
