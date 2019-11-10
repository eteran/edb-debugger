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
#include "Configuration.h"
#include "DialogStrings.h"
#include "IDebugger.h"
#include "IProcess.h"
#include "IRegion.h"
#include "ISymbolManager.h"
#include "MemoryRegions.h"
#include "Module.h"
#include "QtHelper.h"
#include "Symbol.h"
#include "edb.h"

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
#include <arpa/inet.h>
#include <link.h>
#endif

namespace ProcessPropertiesPlugin {

Q_DECLARE_NAMESPACE_TR(ProcessPropertiesPlugin)

namespace {
/**
 * @brief arguments_to_string
 * @param args
 * @return
 */
QString arguments_to_string(const QList<QByteArray> &args) {
	QString ret;

	for (const QByteArray &arg : args) {
		ret.append(' ');
		ret.append(QString::fromUtf8(arg));
	}

	ret.remove(0, 1);
	return ret;
}

/**
 * @brief size_to_string
 * @param n
 * @return
 */
QString size_to_string(size_t n) {

	static constexpr size_t KiB = 1024;
	static constexpr size_t MiB = KiB * 1024;
	static constexpr size_t GiB = MiB * 1024;

	if (n < KiB) {
		return QString::number(n);
	} else if (n < MiB) {
		return QString::number(n / KiB) + tr(" KiB");
	} else if (n < GiB) {
		return QString::number(n / MiB) + tr(" MiB");
	} else {
		return QString::number(n / GiB) + tr(" GiB");
	}
}

#if defined(Q_OS_LINUX)
/**
 * @brief file_type
 * @param filename
 * @return
 */
QString file_type(const QString &filename) {
	const QFileInfo info(filename);
	const QString basename(info.completeBaseName());

	if (basename.startsWith("socket:")) {
		return tr("Socket");
	}

	if (basename.startsWith("pipe:")) {
		return tr("Pipe");
	}

	return tr("File");
}

/**
 * @brief tcp_socket_prcoessor
 * @param symlink
 * @param sock
 * @param lst
 * @return
 */
bool tcp_socket_prcoessor(QString *symlink, int sock, const QStringList &lst) {

	Q_ASSERT(symlink);

	if (lst.size() >= 13) {

		bool ok;
		const uint32_t local_address = ntohl(lst[1].toUInt(&ok, 16));
		if (ok) {
			const uint16_t local_port = lst[2].toUInt(&ok, 16);
			if (ok) {
				const uint32_t remote_address = ntohl(lst[3].toUInt(&ok, 16));
				if (ok) {
					const uint16_t remote_port = lst[4].toUInt(&ok, 16);
					if (ok) {
						const uint8_t state = lst[5].toUInt(&ok, 16);
						Q_UNUSED(state)
						if (ok) {
							const int inode = lst[13].toUInt(&ok, 10);
							if (ok) {
								if (inode == sock) {
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

/**
 * @brief udp_socket_processor
 * @param symlink
 * @param sock
 * @param lst
 * @return
 */
bool udp_socket_processor(QString *symlink, int sock, const QStringList &lst) {

	Q_ASSERT(symlink);

	if (lst.size() >= 13) {

		bool ok;
		const uint32_t local_address = ntohl(lst[1].toUInt(&ok, 16));
		if (ok) {
			const uint16_t local_port = lst[2].toUInt(&ok, 16);
			if (ok) {
				const uint32_t remote_address = ntohl(lst[3].toUInt(&ok, 16));
				if (ok) {
					const uint16_t remote_port = lst[4].toUInt(&ok, 16);
					if (ok) {
						const uint8_t state = lst[5].toUInt(&ok, 16);
						Q_UNUSED(state)
						if (ok) {
							const int inode = lst[13].toUInt(&ok, 10);
							if (ok) {
								if (inode == sock) {
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

/**
 * @brief unix_socket_processor
 * @param symlink
 * @param sock
 * @param lst
 * @return
 */
bool unix_socket_processor(QString *symlink, int sock, const QStringList &lst) {

	Q_ASSERT(symlink);

	if (lst.size() >= 6) {
		bool ok;
		// TODO(eteran): should this be toInt(...)?
		const int inode = lst[6].toUInt(&ok, 10);
		if (ok) {
			if (inode == sock) {
				*symlink = QString("UNIX [%1]").arg(lst[0]);
				return true;
			}
		}
	}
	return false;
}

/**
 * @brief process_socket_file
 * @param filename
 * @param symlink
 * @param sock
 * @param func
 * @return
 */
template <class F>
QString process_socket_file(const QString &filename, QString *symlink, int sock, F func) {

	Q_ASSERT(symlink);

	QFile net(filename);
	net.open(QIODevice::ReadOnly | QIODevice::Text);
	if (net.isOpen()) {
		QTextStream in(&net);
		QString line;

		// ditch first line, it is just table headings
		in.readLine();

		// read in the first line we care about
		line = in.readLine();

		// a null string means end of file (but not an empty string!)
		while (!line.isNull()) {

			QString lline(line);
			const QStringList lst = lline.replace(":", " ").split(" ", QString::SkipEmptyParts);

			if (func(symlink, sock, lst)) {
				break;
			}

			line = in.readLine();
		}
	}
	return *symlink;
}

/**
 * @brief process_socket_tcp
 * @param symlink
 * @return
 */
QString process_socket_tcp(QString *symlink) {

	Q_ASSERT(symlink);

	const QString socket_info(symlink->mid(symlink->indexOf("socket:[")));
	const int socket_number = socket_info.mid(8).remove("]").toUInt();

	return process_socket_file("/proc/net/tcp", symlink, socket_number, tcp_socket_prcoessor);
}

/**
 * @brief process_socket_unix
 * @param symlink
 * @return
 */
QString process_socket_unix(QString *symlink) {

	Q_ASSERT(symlink);

	const QString socket_info(symlink->mid(symlink->indexOf("socket:[")));
	const int socket_number = socket_info.mid(8).remove("]").toUInt();

	return process_socket_file("/proc/net/unix", symlink, socket_number, unix_socket_processor);
}

/**
 * @brief process_socket_udp
 * @param symlink
 * @return
 */
QString process_socket_udp(QString *symlink) {

	Q_ASSERT(symlink);

	const QString socket_info(symlink->mid(symlink->indexOf("socket:[")));
	const int socket_number = socket_info.mid(8).remove("]").toUInt();

	return process_socket_file("/proc/net/udp", symlink, socket_number, udp_socket_processor);
}
#endif

}

/**
 * @brief DialogProcessProperties::DialogProcessProperties
 * @param parent
 * @param f
 */
DialogProcessProperties::DialogProcessProperties(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {

	ui.setupUi(this);
	ui.tableModules->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
	ui.tableMemory->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
	ui.threadTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

	threadsModel_  = new ThreadsModel(this);
	threadsFilter_ = new QSortFilterProxyModel(this);

	threadsFilter_->setSourceModel(threadsModel_);
	threadsFilter_->setFilterCaseSensitivity(Qt::CaseInsensitive);

	ui.threadTable->setModel(threadsFilter_);
}

/**
 * @brief DialogProcessProperties::updateGeneralPage
 */
void DialogProcessProperties::updateGeneralPage() {
	if (edb::v1::debugger_core) {
		if (IProcess *process = edb::v1::debugger_core->process()) {
			const QString exe = process->executable();
			const QString cwd = process->currentWorkingDirectory();

			std::shared_ptr<IProcess> parent = process->parent();
			const edb::pid_t parent_pid      = parent ? parent->pid() : 0;
			const QString parent_exe         = parent ? parent->executable() : QString();

			const QList<QByteArray> args = process->arguments();

			ui.editImage->setText(exe);

			// TODO(eteran): handle arguments with spaces
			ui.editCommand->setText(arguments_to_string(args));
			ui.editCurrentDirectory->setText(cwd);
			ui.editStarted->setText(process->startTime().toString("yyyy-MM-dd hh:mm:ss.z"));
			if (parent_pid) {
				ui.editParent->setText(QString("%1 (%2)").arg(parent_exe).arg(parent_pid));
			} else {
				ui.editParent->setText(QString());
			}
		} else {
			ui.editImage->setText(QString());
			ui.editCommand->setText(QString());
			ui.editCurrentDirectory->setText(QString());
			ui.editStarted->setText(QString());
			ui.editParent->setText(QString());
		}
	}
}

/**
 * @brief DialogProcessProperties::updateModulePage
 */
void DialogProcessProperties::updateModulePage() {

	ui.tableModules->clearContents();
	ui.tableModules->setRowCount(0);
	if (edb::v1::debugger_core) {
		if (IProcess *process = edb::v1::debugger_core->process()) {
			const QList<Module> modules = process->loadedModules();
			ui.tableModules->setSortingEnabled(false);
			for (const Module &m : modules) {
				const int row = ui.tableModules->rowCount();
				ui.tableModules->insertRow(row);
				ui.tableModules->setItem(row, 0, new QTableWidgetItem(edb::v1::format_pointer(m.baseAddress)));
				ui.tableModules->setItem(row, 1, new QTableWidgetItem(m.name));
			}
			ui.tableModules->setSortingEnabled(true);
		}
	}
}

/**
 * @brief DialogProcessProperties::updateMemoryPage
 */
void DialogProcessProperties::updateMemoryPage() {

	ui.tableMemory->clearContents();
	ui.tableMemory->setRowCount(0);

	if (edb::v1::debugger_core) {
		edb::v1::memory_regions().sync();
		const QList<std::shared_ptr<IRegion>> regions = edb::v1::memory_regions().regions();
		ui.tableMemory->setSortingEnabled(false);

		for (const std::shared_ptr<IRegion> &r : regions) {
			const int row = ui.tableMemory->rowCount();
			ui.tableMemory->insertRow(row);
			ui.tableMemory->setItem(row, 0, new QTableWidgetItem(edb::v1::format_pointer(r->start()))); // address
			ui.tableMemory->setItem(row, 1, new QTableWidgetItem(size_to_string(r->size())));           // size
			ui.tableMemory->setItem(row, 2, new QTableWidgetItem(QString("%1%2%3")                      // protection
																	 .arg(r->readable() ? 'r' : '-')
																	 .arg(r->writable() ? 'w' : '-')
																	 .arg(r->executable() ? 'x' : '-')));
			ui.tableMemory->setItem(row, 3, new QTableWidgetItem(r->name())); // name
		}
		ui.tableMemory->setSortingEnabled(true);
	}
}

/**
 * @brief DialogProcessProperties::on_txtSearchEnvironment_textChanged
 * @param text
 */
void DialogProcessProperties::on_txtSearchEnvironment_textChanged(const QString &text) {
	updateEnvironmentPage(text);
}

/**
 * @brief DialogProcessProperties::updateEnvironmentPage
 * @param filter
 */
void DialogProcessProperties::updateEnvironmentPage(const QString &filter) {
	// tableEnvironment

	ui.tableEnvironment->clearContents();
	ui.tableEnvironment->setSortingEnabled(false);
	ui.tableEnvironment->setRowCount(0);

	const QString lower_filter = filter.toLower();

#ifdef Q_OS_LINUX
	if (IProcess *process = edb::v1::debugger_core->process()) {
		QFile proc_environ(QString("/proc/%1/environ").arg(process->pid()));
		if (proc_environ.open(QIODevice::ReadOnly)) {
			QByteArray env = proc_environ.readAll();
			char *p        = env.data();
			char *ptr      = p;
			while (ptr != p + env.size()) {
				const QString env       = QString::fromUtf8(ptr);
				const QString env_name  = env.mid(0, env.indexOf("="));
				const QString env_value = env.mid(env.indexOf("=") + 1);

				if (lower_filter.isEmpty() || env_name.contains(lower_filter, Qt::CaseInsensitive)) {
					const int row = ui.tableEnvironment->rowCount();
					ui.tableEnvironment->insertRow(row);
					ui.tableEnvironment->setItem(row, 0, new QTableWidgetItem(env_name));
					ui.tableEnvironment->setItem(row, 1, new QTableWidgetItem(env_value));
				}

				ptr += qstrlen(ptr) + 1;
			}
		}
	}
#endif

	ui.tableEnvironment->setSortingEnabled(true);
}

/**
 * @brief DialogProcessProperties::updateHandles
 */
void DialogProcessProperties::updateHandles() {

	ui.tableHandles->setSortingEnabled(false);
	ui.tableHandles->setRowCount(0);

#ifdef Q_OS_LINUX
	if (IProcess *process = edb::v1::debugger_core->process()) {
		QDir dir(QString("/proc/%1/fd/").arg(process->pid()));
		const QFileInfoList entries = dir.entryInfoList(QStringList() << "[0-9]*");
		for (const QFileInfo &info : entries) {
			if (info.isSymLink()) {
				QString symlink(info.symLinkTarget());
				const QString type(file_type(symlink));

				if (type == tr("Socket")) {
					symlink = process_socket_tcp(&symlink);
					symlink = process_socket_udp(&symlink);
					symlink = process_socket_unix(&symlink);
				}

				if (type == tr("Pipe")) {
					symlink = tr("FIFO");
				}

				const int row = ui.tableHandles->rowCount();
				ui.tableHandles->insertRow(row);

				auto itemFD = new QTableWidgetItem;
				itemFD->setData(Qt::DisplayRole, info.fileName().toUInt());

				ui.tableHandles->setItem(row, 0, new QTableWidgetItem(type));
				ui.tableHandles->setItem(row, 1, itemFD);
				ui.tableHandles->setItem(row, 2, new QTableWidgetItem(symlink));
			}
		}
	}
#endif

	ui.tableHandles->setSortingEnabled(true);
}

/**
 * @brief DialogProcessProperties::showEvent
 */
void DialogProcessProperties::showEvent(QShowEvent *) {
	updateGeneralPage();
	updateMemoryPage();
	updateModulePage();
	updateHandles();
	updateThreads();
	updateEnvironmentPage(ui.txtSearchEnvironment->text());
}

/**
 * @brief DialogProcessProperties::on_btnParent_clicked
 */
void DialogProcessProperties::on_btnParent_clicked() {

	if (edb::v1::debugger_core) {
		if (IProcess *process = edb::v1::debugger_core->process()) {

			std::shared_ptr<IProcess> parent = process->parent();
			const QString parent_exe         = parent ? parent->executable() : QString();

			QFileInfo info(parent_exe);
			QDir dir = info.absoluteDir();
			QDesktopServices::openUrl(QUrl(tr("file://%1").arg(dir.absolutePath()), QUrl::TolerantMode));
		}
	}
}

/**
 * @brief DialogProcessProperties::on_btnImage_clicked
 */
void DialogProcessProperties::on_btnImage_clicked() {
	if (edb::v1::debugger_core) {
		QFileInfo info(ui.editImage->text());
		QDir dir = info.absoluteDir();
		QDesktopServices::openUrl(QUrl(tr("file://%1").arg(dir.absolutePath()), QUrl::TolerantMode));
	}
}

/**
 * @brief DialogProcessProperties::on_btnRefreshEnvironment_clicked
 */
void DialogProcessProperties::on_btnRefreshEnvironment_clicked() {
	updateEnvironmentPage(ui.txtSearchEnvironment->text());
}

/**
 * @brief DialogProcessProperties::on_btnRefreshHandles_clicked
 */
void DialogProcessProperties::on_btnRefreshHandles_clicked() {
	updateHandles();
}

/**
 * @brief DialogProcessProperties::on_btnStrings_clicked
 */
void DialogProcessProperties::on_btnStrings_clicked() {

	static auto dialog = new DialogStrings(edb::v1::debugger_ui);
	dialog->show();
}

/**
 * @brief DialogProcessProperties::on_btnRefreshMemory_clicked
 */
void DialogProcessProperties::on_btnRefreshMemory_clicked() {
	updateMemoryPage();
}

/**
 * @brief DialogProcessProperties::on_btnRefreshThreads_clicked
 */
void DialogProcessProperties::on_btnRefreshThreads_clicked() {
	updateThreads();
}

/**
 * @brief DialogProcessProperties::updateThreads
 */
void DialogProcessProperties::updateThreads() {
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
}

}
