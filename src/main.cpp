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

#include "Configuration.h"
#include "Debugger.h"
#include "DebuggerInternal.h"
#include "IDebugger.h"
#include "IPlugin.h"
#include "edb.h"
#include "version.h"

#include <QApplication>
#include <QDir>
#include <QLibrary>
#include <QLibraryInfo>
#include <QMessageBox>
#include <QPluginLoader>
#include <QTranslator>
#include <QtDebug>

#include <ctime>
#include <iostream>

namespace {

//------------------------------------------------------------------------------
// Name: load_plugins
// Desc: attempts to load all plugins in a given directory
//------------------------------------------------------------------------------
void load_plugins(const QString &directory) {

	QDir plugins_dir(qApp->applicationDirPath());

	// TODO(eteran): attempt to detect the same plugin being loaded twice
	// NOTE(eteran): if the plugins directory doesn't exist at all, this CD
	//               will fail and stay in the current directory. This actually
	//               is VERY nice behavior for us since it will allow
	//               running from the build directory without further config
	plugins_dir.cd(directory);

	for(const QString &file_name: plugins_dir.entryList(QDir::Files)) {
		if(QLibrary::isLibrary(file_name)) {
			const QString full_path = plugins_dir.absoluteFilePath(file_name);
			QPluginLoader loader(full_path);
			loader.setLoadHints(QLibrary::ExportExternalSymbolsHint);

			if(QObject *const plugin = loader.instance()) {

				// TODO: handle the case where we find more than one core plugin...
				if(auto core_plugin = qobject_cast<IDebugger *>(plugin)) {
					if(!edb::v1::debugger_core) {
						edb::v1::debugger_core = core_plugin;
					}
				} else if(qobject_cast<IPlugin *>(plugin)) {
					if(edb::internal::register_plugin(full_path, plugin)) {
					}
				}
			} else {
				qDebug() << "[load_plugins]" << qPrintable(loader.errorString());
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: start_debugger
// Desc: starts the main debugger code
//------------------------------------------------------------------------------
int start_debugger(edb::pid_t attach_pid, const QString &program, const QList<QByteArray> &programArgs) {

	qDebug() << "Starting edb version:" << edb::version;
	qDebug("Please Report Bugs & Requests At: https://github.com/eteran/edb-debugger/issues");

	edb::internal::load_function_db();

	// create the main window object
	Debugger debugger;

	// ok things are initialized to a reasonable degree, let's show the main window
	debugger.show();

	if(!edb::v1::debugger_core) {
		QMessageBox::warning(
			0,
			QT_TRANSLATE_NOOP("edb", "edb Failed To Load A Necessary Plugin"),
			QT_TRANSLATE_NOOP("edb",
				"Failed to successfully load the debugger core plugin. Please make sure it exists and that the plugin path is correctly configured.\n"
				"This is normal if edb has not been previously run or the configuration file has been removed."));

		edb::v1::dialog_options()->exec();

		QMessageBox::warning(
			0,
			QT_TRANSLATE_NOOP("edb", "edb"),
			QT_TRANSLATE_NOOP("edb", "edb will now close. If you were successful in specifying the location of the debugger core plugin, please run edb again.")
			);

		// TODO: detect if they corrected the issue and try again
		return -1;
	} else {
		// have we been asked to attach to a given program?
		if(attach_pid != 0) {
			debugger.attach(attach_pid);
		} else if(!program.isEmpty()) {
			debugger.execute(program, programArgs);
		}

		return qApp->exec();
	}
}

//------------------------------------------------------------------------------
// Name: load_translations
// Desc:
//------------------------------------------------------------------------------
void load_translations() {
	// load some translations
	QTranslator qtTranslator;
	qtTranslator.load("qt_" + QLocale::system().name(), QLibraryInfo::location(QLibraryInfo::TranslationsPath));
	qApp->installTranslator(&qtTranslator);

	QTranslator myappTranslator;
	myappTranslator.load("edb_" + QLocale::system().name());
	qApp->installTranslator(&myappTranslator);
}

//------------------------------------------------------------------------------
// Name: usage
// Desc: displays a usage statement then exits
//------------------------------------------------------------------------------
void usage() {

	QStringList args = qApp->arguments();
	std::cerr << "Usage: " << qPrintable(args[0]) << " [OPTIONS]" << std::endl;
	std::cerr << std::endl;
	std::cerr << " --attach <pid>            : attach to running process" << std::endl;
	std::cerr << " --run <program> (args...) : execute specified <program> with <args>" << std::endl;
	std::cerr << " --version                 : output version information and exit" << std::endl;
	std::cerr << " --dump-version            : display terse version string and exit" << std::endl;
	std::cerr << " --help                    : display this help and exit" << std::endl;

	for(QObject *plugin: edb::v1::plugin_list()) {
		if(auto p = qobject_cast<IPlugin *>(plugin)) {
			const QString s = p->extra_arguments();
			if(!s.isEmpty()) {
				std::cerr << std::endl;
				std::cerr << qPrintable(plugin->metaObject()->className()) << std::endl;
				std::cerr << qPrintable(s) << std::endl;
			}
		}
	}

	std::exit(-1);
}

}


//------------------------------------------------------------------------------
// Name: main
// Desc: entry point
//------------------------------------------------------------------------------
int main(int argc, char *argv[]) {

	QT_REQUIRE_VERSION(argc, argv, "4.6.0");

	QApplication app(argc, argv);
	QApplication::setWindowIcon(QIcon(":/debugger/images/edb48-logo.png"));

	qsrand(std::time(0));

	// setup organization info so settings go in right place
	QApplication::setOrganizationName("codef00.com");
	QApplication::setOrganizationDomain("codef00.com");
	QApplication::setApplicationName("edb");
	QApplication::setApplicationVersion(edb::version);

	load_translations();

	// look for some plugins..
	load_plugins(edb::v1::config().plugin_path);

	QStringList args = app.arguments();
	edb::pid_t        attach_pid = 0;
	QList<QByteArray> run_args;
	QString           run_app;

	// call the init function for each plugin, this is done after
	// ALL plugins are loaded in case there are inter-plugin dependencies
	for(QObject *plugin: edb::v1::plugin_list()) {
		if(auto p = qobject_cast<IPlugin *>(plugin)) {

			const IPlugin::ArgumentStatus r = p->parse_argments(args);
			switch(r) {
			case IPlugin::ARG_ERROR:
				usage();
				break;
			case IPlugin::ARG_EXIT:
				std::exit(0);
				break;
			default:
				break;
			}
		}
	}

	if(args.size() > 1) {
		if(args.size() == 3 && args[1] == "--attach") {
			attach_pid = args[2].toUInt();
		} else if(args.size() >= 3 && args[1] == "--run") {
			run_app = args[2];

			for(int i = 3; i < args.size(); ++i) {
				run_args.push_back(argv[i]);
			}
		} else if(args.size() == 2 && args[1] == "--version") {
			std::cout << "edb version: " << edb::version << std::endl;
			return 0;
		} else if(args.size() == 2 && args[1] == "--dump-version") {
			std::cout << edb::version << std::endl;
			return 0;
		} else {
			usage();
		}
	}

	return start_debugger(attach_pid, run_app, run_args);
}
