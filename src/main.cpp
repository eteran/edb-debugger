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
#include "QtHelper.h"
#include "Theme.h"
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

#include <boost/optional.hpp>

namespace {

Q_DECLARE_NAMESPACE_TR(edb)

struct LaunchArguments {

	// if we are attaching
	boost::optional<edb::pid_t> attach_pid;

	// if we are running
	QList<QByteArray> run_args;
	QString run_app;
	QString run_stdin;
	QString run_stdout;
};

/**
 * displays a usage statement then exits
 *
 * @brief usage
 */
[[noreturn]] void usage() {

	QStringList args = qApp->arguments();
	std::cerr << "Usage: " << qPrintable(args[0]) << " [OPTIONS]\n";
	std::cerr << '\n';
	std::cerr << " --attach <pid>            : attach to running process\n";
	std::cerr << " --run <program> (args...) : execute specified <program> with <args>\n";
	std::cerr << " --stdin <filename>        : set the STDIN of the target process (MUST preceed --run)\n";
	std::cerr << " --stdout <filename>       : set the STDOUT of the target process (MUST preceed --run)\n";
	std::cerr << " --version                 : output version information and exit\n";
	std::cerr << " --dump-version            : display terse version string and exit\n";
	std::cerr << " --help                    : display this help and exit\n";

	for (QObject *plugin : edb::v1::plugin_list()) {
		if (auto p = qobject_cast<IPlugin *>(plugin)) {
			const QString s = p->extraArguments();
			if (!s.isEmpty()) {
				std::cerr << '\n';
				std::cerr << qPrintable(plugin->metaObject()->className()) << '\n';
				std::cerr << qPrintable(s) << '\n';
			}
		}
	}

	std::cerr << std::flush;
	std::exit(-1);
}

/**
 * @brief validate_launch_arguments
 * @param launch_args
 */
void validate_launch_arguments(const LaunchArguments &launch_args) {
	if (!launch_args.run_app.isEmpty() && launch_args.attach_pid) {
		std::cerr << "ERROR: Cannot specify both --attach and --run\n\n";
		usage();
	}

	if ((!launch_args.run_stdin.isEmpty() || !launch_args.run_stdout.isEmpty()) && launch_args.run_app.isEmpty()) {
		std::cerr << "ERROR: --stdin and --stdout MUST be combined with --run\n\n";
		usage();
	}
}

/**
 * attempts to load all plugins in a given directory
 *
 * @brief load_plugins
 * @param directory
 */
void load_plugins(const QString &directory) {

	QDir plugins_dir(qApp->applicationDirPath());

	// TODO(eteran): attempt to detect the same plugin being loaded twice
	// NOTE(eteran): if the plugins directory doesn't exist at all, this CD
	//               will fail and stay in the current directory. This actually
	//               is VERY nice behavior for us since it will allow
	//               running from the build directory without further config
	plugins_dir.cd(directory);

	Q_FOREACH (const QString &file_name, plugins_dir.entryList(QDir::Files)) {
		if (QLibrary::isLibrary(file_name)) {
			const QString full_path = plugins_dir.absoluteFilePath(file_name);
			QPluginLoader loader(full_path);
			loader.setLoadHints(QLibrary::ExportExternalSymbolsHint);

			if (QObject *const plugin = loader.instance()) {
				if (auto core_plugin = qobject_cast<IDebugger *>(plugin)) {
					if (!edb::v1::debugger_core) {
						edb::v1::debugger_core = core_plugin;

						// load in the settings that the core needs
						edb::v1::debugger_core->setIgnoredExceptions(edb::v1::config().ignored_exceptions);
					}
				} else if (qobject_cast<IPlugin *>(plugin)) {
					if (edb::internal::register_plugin(full_path, plugin)) {
					}
				}
			} else {
				qDebug() << "[load_plugins]" << qPrintable(loader.errorString());
			}
		}
	}
}

/**
 * starts the main debugger code
 *
 * @brief start_debugger
 * @param launch_args
 * @return
 */
int start_debugger(const LaunchArguments &launch_args) {

	qDebug() << "Starting edb version:" << edb::version;
	qDebug("Please Report Bugs & Requests At: https://github.com/eteran/edb-debugger/issues");

	edb::internal::load_function_db();

	// create the main window object
	Debugger debugger;

	// ok things are initialized to a reasonable degree, let's show the main window
	debugger.show();

	if (!edb::v1::debugger_core) {
		QMessageBox::warning(
			nullptr,
			tr("edb Failed To Load A Necessary Plugin"),
			tr("Failed to successfully load the debugger core plugin. Please make sure it exists and that the plugin path is correctly configured.\n"
			   "This is normal if edb has not been previously run or the configuration file has been removed."));

		edb::v1::dialog_options()->exec();

		QMessageBox::warning(
			nullptr,
			tr("edb"),
			tr("edb will now close. If you were successful in specifying the location of the debugger core plugin, please run edb again."));

		// TODO: detect if they corrected the issue and try again
		return -1;
	} else {
		// have we been asked to attach to a given program?
		if (launch_args.attach_pid) {
			debugger.attach(*launch_args.attach_pid);
		} else if (!launch_args.run_app.isEmpty()) {
			debugger.execute(launch_args.run_app, launch_args.run_args, launch_args.run_stdin, launch_args.run_stdout);
		}

		return qApp->exec();
	}
}

/**
 * @brief load_translations
 */
void load_translations() {
	// load some translations
	QTranslator qtTranslator;
	qtTranslator.load("qt_" + QLocale::system().name(), QLibraryInfo::location(QLibraryInfo::TranslationsPath));
	qApp->installTranslator(&qtTranslator);

	QTranslator myappTranslator;
	myappTranslator.load("edb_" + QLocale::system().name());
	qApp->installTranslator(&myappTranslator);
}

// See QtCreator: src/libs/utils/theme/theme.cpp

// If you copy QPalette, default values stay at default, even if that default is different
// within the context of different widgets. Create deep copy.
QPalette copyPalette(const QPalette &p) {
	QPalette res;
	for (int group = 0; group < QPalette::NColorGroups; ++group) {
		for (int role = 0; role < QPalette::NColorRoles; ++role) {
			res.setBrush(QPalette::ColorGroup(group),
						 QPalette::ColorRole(role),
						 p.brush(QPalette::ColorGroup(group), QPalette::ColorRole(role)));
		}
	}
	return res;
}

QPalette initialPalette() {
	static QPalette palette = copyPalette(QApplication::palette());
	return palette;
}

QPalette themePalette() {

	QPalette pal = initialPalette();

	Theme theme = Theme::load();

	const static struct {
		Theme::Palette paletteIndex;
		QPalette::ColorRole paletteColorRole;
		QPalette::ColorGroup paletteColorGroup;
		bool setColorRoleAsBrush;
	} mapping[] = {
		{Theme::Palette::Window, QPalette::Window, QPalette::All, false},
		{Theme::Palette::WindowDisabled, QPalette::Window, QPalette::Disabled, false},
		{Theme::Palette::WindowText, QPalette::WindowText, QPalette::All, true},
		{Theme::Palette::WindowTextDisabled, QPalette::WindowText, QPalette::Disabled, true},
		{Theme::Palette::Base, QPalette::Base, QPalette::All, false},
		{Theme::Palette::BaseDisabled, QPalette::Base, QPalette::Disabled, false},
		{Theme::Palette::AlternateBase, QPalette::AlternateBase, QPalette::All, false},
		{Theme::Palette::AlternateBaseDisabled, QPalette::AlternateBase, QPalette::Disabled, false},
		{Theme::Palette::ToolTipBase, QPalette::ToolTipBase, QPalette::All, true},
		{Theme::Palette::ToolTipBaseDisabled, QPalette::ToolTipBase, QPalette::Disabled, true},
		{Theme::Palette::ToolTipText, QPalette::ToolTipText, QPalette::All, false},
		{Theme::Palette::ToolTipTextDisabled, QPalette::ToolTipText, QPalette::Disabled, false},
		{Theme::Palette::Text, QPalette::Text, QPalette::All, true},
		{Theme::Palette::TextDisabled, QPalette::Text, QPalette::Disabled, true},
		{Theme::Palette::Button, QPalette::Button, QPalette::All, false},
		{Theme::Palette::ButtonDisabled, QPalette::Button, QPalette::Disabled, false},
		{Theme::Palette::ButtonText, QPalette::ButtonText, QPalette::All, true},
		{Theme::Palette::ButtonTextDisabled, QPalette::ButtonText, QPalette::Disabled, true},
		{Theme::Palette::BrightText, QPalette::BrightText, QPalette::All, false},
		{Theme::Palette::BrightTextDisabled, QPalette::BrightText, QPalette::Disabled, false},
		{Theme::Palette::Highlight, QPalette::Highlight, QPalette::All, true},
		{Theme::Palette::HighlightDisabled, QPalette::Highlight, QPalette::Disabled, true},
		{Theme::Palette::HighlightedText, QPalette::HighlightedText, QPalette::All, true},
		{Theme::Palette::HighlightedTextDisabled, QPalette::HighlightedText, QPalette::Disabled, true},
		{Theme::Palette::Link, QPalette::Link, QPalette::All, false},
		{Theme::Palette::LinkDisabled, QPalette::Link, QPalette::Disabled, false},
		{Theme::Palette::LinkVisited, QPalette::LinkVisited, QPalette::All, false},
		{Theme::Palette::LinkVisitedDisabled, QPalette::LinkVisited, QPalette::Disabled, false},
		{Theme::Palette::Light, QPalette::Light, QPalette::All, false},
		{Theme::Palette::LightDisabled, QPalette::Light, QPalette::Disabled, false},
		{Theme::Palette::Midlight, QPalette::Midlight, QPalette::All, false},
		{Theme::Palette::MidlightDisabled, QPalette::Midlight, QPalette::Disabled, false},
		{Theme::Palette::Dark, QPalette::Dark, QPalette::All, false},
		{Theme::Palette::DarkDisabled, QPalette::Dark, QPalette::Disabled, false},
		{Theme::Palette::Mid, QPalette::Mid, QPalette::All, false},
		{Theme::Palette::MidDisabled, QPalette::Mid, QPalette::Disabled, false},
		{Theme::Palette::Shadow, QPalette::Shadow, QPalette::All, false},
		{Theme::Palette::ShadowDisabled, QPalette::Shadow, QPalette::Disabled, false}};

	for (auto entry : mapping) {
		const QColor themeColor = theme.palette[entry.paletteIndex];
		// Use original color if color is not defined in theme.
		if (themeColor.isValid()) {
			if (entry.setColorRoleAsBrush)
				// TODO: Find out why sometimes setBrush is used
				pal.setBrush(entry.paletteColorGroup, entry.paletteColorRole, themeColor);
			else
				pal.setColor(entry.paletteColorGroup, entry.paletteColorRole, themeColor);
		}
	}

	return pal;
}

}

/**
 * @brief main
 * @param argc
 * @param argv
 * @return
 */
int main(int argc, char *argv[]) {

	QT_REQUIRE_VERSION(argc, argv, "5.2.0");

#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
	QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
	QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

	QApplication app(argc, argv);
	QApplication::setWindowIcon(QIcon(":/debugger/images/edb48-logo.png"));

	qsrand(std::time(nullptr));

	// setup organization info so settings go in right place
	QApplication::setOrganizationName("codef00.com");
	QApplication::setOrganizationDomain("codef00.com");
	QApplication::setApplicationName("edb");
	QApplication::setApplicationVersion(edb::version);

	load_translations();

	// look for some plugins..
	load_plugins(edb::v1::config().plugin_path);

	QStringList args = app.arguments();

	// call the parseArguments function for each plugin
	for (QObject *plugin : edb::v1::plugin_list()) {
		if (auto p = qobject_cast<IPlugin *>(plugin)) {

			const IPlugin::ArgumentStatus r = p->parseArguments(args);
			switch (r) {
			case IPlugin::ARG_ERROR:
				usage();
			case IPlugin::ARG_EXIT:
				std::exit(0);
			default:
				break;
			}
		}
	}

	LaunchArguments launch_args;

	for (int i = 1; i < args.size(); ++i) {

		if (args[i] == "--version") {
			std::cout << "edb version: " << edb::version << std::endl;
			return 0;
		} else if (args[i] == "--dump-version") {
			std::cout << edb::version << std::endl;
			return 0;
		} else if (args[i] == "--attach") {
			++i;
			if (i >= args.size()) {
				usage();
			}
			launch_args.attach_pid = args[i].toUInt();
		} else if (args[i] == "--run") {
			++i;
			if (i >= args.size()) {
				usage();
			}

			launch_args.run_app = args[i++];

			for (; i < args.size(); ++i) {
				// NOTE(eteran): we are using argv here, not args, because
				// we want to avoid any unicode conversions
				launch_args.run_args.push_back(argv[i]);
			}
		} else if (args[i] == "--stdin") {
			++i;
			if (i >= args.size()) {
				usage();
			}
			launch_args.run_stdin = args[i];
		} else if (args[i] == "--stdout") {
			++i;
			if (i >= args.size()) {
				usage();
			}
			launch_args.run_stdout = args[i];
		} else {
			usage();
		}
	}

	validate_launch_arguments(launch_args);

	QApplication::setPalette(themePalette());

	// Light/Dark icons on all platforms
	if (QApplication::palette().window().color().lightnessF() >= 0.5) {
		QIcon::setThemeName(QLatin1String("breeze-edb"));
	} else {
		QIcon::setThemeName(QLatin1String("breeze-dark-edb"));
	}

	return start_debugger(launch_args);
}
