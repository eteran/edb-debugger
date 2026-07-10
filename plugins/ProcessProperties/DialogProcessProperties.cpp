/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
 * @brief Formats a list of command-line arguments into a single string, with each argument separated by a space.
 *
 * @param args The list of command-line arguments to be formatted.
 * @return A string containing the formatted command-line arguments.
 */
QString arguments_to_string(const QList<QByteArray> &args) {
	QString ret;

	for (const QByteArray &arg : args) {
		ret.append(QLatin1Char(' '));
		ret.append(QString::fromUtf8(arg));
	}

	ret.remove(0, 1);
	return ret;
}

/**
 * @brief Converts a size in bytes to a human-readable string with appropriate units.
 *
 * @param n The size in bytes.
 * @return A string containing the formatted size.
 */
QString size_to_string(size_t n) {

	static constexpr size_t KiB = 1024;
	static constexpr size_t MiB = KiB * 1024;
	static constexpr size_t GiB = MiB * 1024;

	if (n < KiB) {
		return QString::number(n);
	}

	if (n < MiB) {
		return QString::number(n / KiB) + tr(" KiB");
	}

	if (n < GiB) {
		return QString::number(n / MiB) + tr(" MiB");
	}

	return QString::number(n / GiB) + tr(" GiB");
}

#if defined(Q_OS_LINUX)
/**
 * @brief Determines the type of a file based on its name.
 *
 * @param filename The name of the file.
 * @return A string indicating the type of the file.
 */
QString file_type(const QString &filename) {
	const QFileInfo info(filename);
	const QString basename(info.completeBaseName());

	if (basename.startsWith(QLatin1String("socket:"))) {
		return tr("Socket");
	}

	if (basename.startsWith(QLatin1String("pipe:"))) {
		return tr("Pipe");
	}

	return tr("File");
}

/**
 * @brief Processes a TCP socket file to extract and format its information.
 *
 * @param symlink Where the formatted socket information will be stored.
 * @param sock The socket number to be processed.
 * @param lst A list of strings containing the socket information.
 * @return True if the socket information was successfully processed and formatted, false otherwise.
 */
bool tcp_socket_processor(QString *symlink, int sock, const QStringList &lst) {

	Q_ASSERT(symlink);

	if (lst.size() >= 13) {

		bool ok;
		const uint32_t local_address = ntohl(lst[1].toUInt(&ok, 16));
		if (ok) {
			const auto local_port = static_cast<uint16_t>(lst[2].toUInt(&ok, 16));
			if (ok) {
				const uint32_t remote_address = ntohl(lst[3].toUInt(&ok, 16));
				if (ok) {
					const auto remote_port = static_cast<uint16_t>(lst[4].toUInt(&ok, 16));
					if (ok) {
						const auto state = static_cast<uint8_t>(lst[5].toUInt(&ok, 16));
						Q_UNUSED(state)
						if (ok) {
							const auto inode = static_cast<int>(lst[13].toUInt(&ok, 10));
							if (ok) {
								if (inode == sock) {
									*symlink = QStringLiteral("TCP: %1:%2 -> %3:%4")
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
 * @brief Processes a UDP socket file to extract and format its information.
 *
 * @param symlink Where the formatted socket information will be stored.
 * @param sock The socket number to be processed.
 * @param lst A list of strings containing the socket information.
 * @return True if the socket information was successfully processed and formatted, false otherwise.
 */
bool udp_socket_processor(QString *symlink, int sock, const QStringList &lst) {

	Q_ASSERT(symlink);

	if (lst.size() >= 13) {

		bool ok;
		const uint32_t local_address = ntohl(lst[1].toUInt(&ok, 16));
		if (ok) {
			const auto local_port = static_cast<uint16_t>(lst[2].toUInt(&ok, 16));
			if (ok) {
				const uint32_t remote_address = ntohl(lst[3].toUInt(&ok, 16));
				if (ok) {
					const auto remote_port = static_cast<uint16_t>(lst[4].toUInt(&ok, 16));
					if (ok) {
						const auto state = static_cast<uint8_t>(lst[5].toUInt(&ok, 16));
						Q_UNUSED(state)
						if (ok) {
							const auto inode = static_cast<int>(lst[13].toUInt(&ok, 10));
							if (ok) {
								if (inode == sock) {
									*symlink = QStringLiteral("UDP: %1:%2 -> %3:%4")
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
 * @brief Processes a UNIX socket file to extract and format its information.
 *
 * @param symlink Where the formatted socket information will be stored.
 * @param sock The socket number to be processed.
 * @param lst A list of strings containing the socket information.
 * @return True if the socket information was successfully processed and formatted, false otherwise.
 */
bool unix_socket_processor(QString *symlink, int sock, const QStringList &lst) {

	Q_ASSERT(symlink);

	if (lst.size() >= 6) {
		bool ok;
		// TODO(eteran): should this be toInt(...)?
		const int inode = lst[6].toUInt(&ok, 10);
		if (ok) {
			if (inode == sock) {
				*symlink = QStringLiteral("UNIX [%1]").arg(lst[0]);
				return true;
			}
		}
	}
	return false;
}

/**
 * @brief Processes a socket file to extract and format its information based on the specified file type (TCP, UDP, or UNIX).
 *
 * @param filename The path to the socket file to be processed.
 * @param symlink Where the formatted socket information will be stored.
 * @param sock The socket number to be processed.
 * @param func A function that processes the socket information based on the file type.
 * @return A string containing the formatted socket information.
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
			const QStringList lst = lline.replace(QLatin1Char(':'), QLatin1Char(' ')).split(QLatin1Char(' '), Qt::SkipEmptyParts);

			if (func(symlink, sock, lst)) {
				break;
			}

			line = in.readLine();
		}
	}
	return *symlink;
}

/**
 * @brief Processes a TCP socket to extract and format its information.
 *
 * @param symlink Where the formatted socket information will be stored.
 * @return A string containing the formatted TCP socket information.
 */
QString process_socket_tcp(QString *symlink) {

	Q_ASSERT(symlink);

	const QString socket_info(symlink->mid(symlink->indexOf(QLatin1String("socket:["))));
	const int socket_number = socket_info.mid(8).remove(QLatin1Char(']')).toUInt();

	return process_socket_file(QStringLiteral("/proc/net/tcp"), symlink, socket_number, tcp_socket_processor);
}

/**
 * @brief Processes a UNIX socket to extract and format its information.
 *
 * @param symlink Where the formatted socket information will be stored.
 * @return A string containing the formatted UNIX socket information.
 */
QString process_socket_unix(QString *symlink) {

	Q_ASSERT(symlink);

	const QString socket_info(symlink->mid(symlink->indexOf(QLatin1String("socket:["))));
	const int socket_number = socket_info.mid(8).remove(QLatin1Char(']')).toUInt();

	return process_socket_file(QStringLiteral("/proc/net/unix"), symlink, socket_number, unix_socket_processor);
}

/**
 * @brief Processes a UDP socket to extract and format its information.
 *
 * @param symlink Where the formatted socket information will be stored.
 * @return A string containing the formatted UDP socket information.
 */
QString process_socket_udp(QString *symlink) {

	Q_ASSERT(symlink);

	const QString socket_info(symlink->mid(symlink->indexOf(QLatin1String("socket:["))));
	const int socket_number = socket_info.mid(8).remove(QLatin1Char(']')).toUInt();

	return process_socket_file(QStringLiteral("/proc/net/udp"), symlink, socket_number, udp_socket_processor);
}
#endif

}

/**
 * @brief Constructor for the DialogProcessProperties class.
 *
 * @param parent The parent widget of the dialog.
 * @param f The window flags for the dialog.
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
 * @brief Updates the general information page of the process properties dialog with the current process's details,
 * such as executable path, command-line arguments, current working directory, start time, and parent process information.
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
			ui.editStarted->setText(process->startTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss.z")));
			if (parent_pid) {
				ui.editParent->setText(QStringLiteral("%1 (%2)").arg(parent_exe).arg(parent_pid));
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
 * @brief Updates the module page of the process properties dialog with the list of loaded modules for the current process.
 * It retrieves the loaded modules from the debugger core and populates the table with their base addresses and names.
 */
void DialogProcessProperties::updateModulePage() {

	ui.tableModules->clearContents();
	ui.tableModules->setRowCount(0);
	if (edb::v1::debugger_core) {
		if (IProcess *process = edb::v1::debugger_core->process()) {
			const QSet<Module> modules = process->loadedModules();
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
 * @brief Updates the memory page of the process properties dialog with the list of memory regions for the current process.
 * It retrieves the memory regions from the debugger core and populates the table with their start addresses, sizes, protection flags, and names.
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
			ui.tableMemory->setItem(row, 2, new QTableWidgetItem(QStringLiteral("%1%2%3")               // protection
																	 .arg(r->readable() ? QLatin1Char('r') : QLatin1Char('-'))
																	 .arg(r->writable() ? QLatin1Char('w') : QLatin1Char('-'))
																	 .arg(r->executable() ? QLatin1Char('x') : QLatin1Char('-'))));
			ui.tableMemory->setItem(row, 3, new QTableWidgetItem(r->name())); // name
		}
		ui.tableMemory->setSortingEnabled(true);
	}
}

/**
 * @brief Handles the text change event for the environment search box.
 * When the text in the search box changes, this slot is triggered, and it updates the environment page.
 *
 * @param text The new text entered in the search box.
 */
void DialogProcessProperties::on_txtSearchEnvironment_textChanged(const QString &text) {
	updateEnvironmentPage(text);
}

/**
 * @brief Updates the environment page of the process properties dialog with the list of environment variables for the current process.
 *
 * @param filter A string used to filter the displayed environment variables. Only variables containing this string (case-insensitive) will be shown.
 */
void DialogProcessProperties::updateEnvironmentPage(const QString &filter) {
	// tableEnvironment

	ui.tableEnvironment->clearContents();
	ui.tableEnvironment->setSortingEnabled(false);
	ui.tableEnvironment->setRowCount(0);

	const QString lower_filter = filter.toLower();

#ifdef Q_OS_LINUX
	if (IProcess *process = edb::v1::debugger_core->process()) {
		QFile proc_environ(QStringLiteral("/proc/%1/environ").arg(process->pid()));
		if (proc_environ.open(QIODevice::ReadOnly)) {
			QByteArray env = proc_environ.readAll();
			char *p        = env.data();
			char *ptr      = p;
			while (ptr != p + env.size()) {
				const auto env          = QString::fromUtf8(ptr);
				const QString env_name  = env.mid(0, env.indexOf(QLatin1Char('=')));
				const QString env_value = env.mid(env.indexOf(QLatin1Char('=')) + 1);

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
 * @brief Updates the handles page of the process properties dialog with the list of open file handles for the current process.
 * It retrieves the open file handles from the debugger core and populates the table with their types, file descriptors, and paths.
 */
void DialogProcessProperties::updateHandles() {

	ui.tableHandles->setSortingEnabled(false);
	ui.tableHandles->setRowCount(0);

#ifdef Q_OS_LINUX
	if (IProcess *process = edb::v1::debugger_core->process()) {
		QDir dir(QStringLiteral("/proc/%1/fd/").arg(process->pid()));
		const QFileInfoList entries = dir.entryInfoList(QStringList() << QStringLiteral("[0-9]*"));
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
 * @brief Handles the show event for the dialog.
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
 * @brief Handles the click event for the "Parent" button.
 * When the button is clicked, this slot is triggered, and it opens the directory containing the parent process's executable in the system's file explorer.
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
 * @brief Handles the click event for the "Image" button.
 * When the button is clicked, this slot is triggered, and it opens the directory containing the current process's executable in the system's file explorer.
 */
void DialogProcessProperties::on_btnImage_clicked() {
	if (edb::v1::debugger_core) {
		QFileInfo info(ui.editImage->text());
		QDir dir = info.absoluteDir();
		QDesktopServices::openUrl(QUrl(tr("file://%1").arg(dir.absolutePath()), QUrl::TolerantMode));
	}
}

/**
 * @brief Handles the click event for the "Refresh Environment" button.
 * When the button is clicked, this slot is triggered, and it updates the environment page with the latest environment variables.
 */
void DialogProcessProperties::on_btnRefreshEnvironment_clicked() {
	updateEnvironmentPage(ui.txtSearchEnvironment->text());
}

/**
 * @brief Handles the click event for the "Refresh Handles" button.
 * When the button is clicked, this slot is triggered, and it updates the handles page with the latest open file handles.
 */
void DialogProcessProperties::on_btnRefreshHandles_clicked() {
	updateHandles();
}

/**
 * @brief Handles the click event for the "Strings" button.
 * When the button is clicked, this slot is triggered, and it updates the strings page.
 */
void DialogProcessProperties::on_btnStrings_clicked() {

	static auto dialog = new DialogStrings(edb::v1::debugger_ui);
	dialog->show();
}

/**
 * @brief Handles the click event for the "Refresh Memory" button.
 * When the button is clicked, this slot is triggered, and it updates the memory page with the latest memory regions.
 */
void DialogProcessProperties::on_btnRefreshMemory_clicked() {
	updateMemoryPage();
}

/**
 * @brief Handles the click event for the "Refresh Threads" button.
 * When the button is clicked, this slot is triggered, and it updates the threads page with the latest thread information.
 */
void DialogProcessProperties::on_btnRefreshThreads_clicked() {
	updateThreads();
}

/**
 * @brief Updates the threads page of the process properties dialog with the list of threads for the current process.
 * It retrieves the threads from the debugger core and populates the threads model with their information, marking the current thread appropriately.
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
