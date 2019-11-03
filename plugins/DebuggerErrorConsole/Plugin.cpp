/*
Copyright (C) 2017 Ruslan Kabatsayev <b7.10110111@gmail.com>

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

#include "Plugin.h"
#include "edb.h"
#include <QDebug>
#include <QDockWidget>
#include <QMainWindow>
#include <QMenu>
#include <QPlainTextEdit>
#include <iostream>

namespace DebuggerErrorConsolePlugin {

namespace {
QPointer<Plugin> instance = nullptr;
}

/**
 * @brief Plugin::debugMessageIntercept
 * @param type
 * @param message
 */
void Plugin::debugMessageIntercept(QtMsgType type, const QMessageLogContext &, const QString &message) {

	if (!instance) {
		return;
	}

	const QString text = [type, &message]() {
		switch (type) {
		case QtDebugMsg:
			return tr("DEBUG %1").arg(message);
#if QT_VERSION >= QT_VERSION_CHECK(5, 5, 0)
		case QtInfoMsg:
			return tr("INFO  %1").arg(message);
#endif
		case QtWarningMsg:
			return tr("WARN  %1").arg(message);
		case QtCriticalMsg:
			return tr("ERROR %1").arg(message);
		case QtFatalMsg:
			return tr("FATAL %1").arg(message);
		default:
			Q_UNREACHABLE();
		}
	}();

	instance->textWidget_->appendPlainText(text);
	std::cerr << message.toUtf8().constData() << "\n"; // this may be useful as a crash log
}

/**
 * @brief Plugin::Plugin
 * @param parent
 */
Plugin::Plugin(QObject *parent)
	: QObject(parent) {

	instance = this;

	textWidget_ = new QPlainTextEdit;
	textWidget_->setReadOnly(true);
	QFont font("monospace");
	font.setStyleHint(QFont::TypeWriter);
	textWidget_->setFont(font);
	textWidget_->setWordWrapMode(QTextOption::WrapMode::NoWrap);
	textWidget_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

	qInstallMessageHandler(debugMessageIntercept);
}

/**
 * @brief Plugin::menu
 * @param parent
 * @return
 */
QMenu *Plugin::menu(QWidget *parent) {
	if (!menu_) {
		if (auto mainWindow = qobject_cast<QMainWindow *>(edb::v1::debugger_ui)) {
			auto dockWidget = new QDockWidget(tr("Debugger Error Console"), mainWindow);
			dockWidget->setObjectName(QString::fromUtf8("Debugger Error Console"));
			dockWidget->setWidget(textWidget_);

			mainWindow->addDockWidget(Qt::BottomDockWidgetArea, dockWidget);

			menu_ = new QMenu(tr("Debugger Error Console"), parent);
			menu_->addAction(dockWidget->toggleViewAction());

			auto docks = mainWindow->findChildren<QDockWidget *>();

			// We want to put it to the same area as Stack dock
			// Stupid QList doesn't have a reverse iterator
			for (auto it = docks.end() - 1;; --it) {
				QDockWidget *const widget = *it;

				if (widget != dockWidget && mainWindow->dockWidgetArea(widget) == Qt::BottomDockWidgetArea) {
					mainWindow->tabifyDockWidget(widget, dockWidget);
					widget->show();
					widget->raise();
					break;
				}

				if (it == docks.begin()) {
					break;
				}
			}
		}
	}
	return menu_;
}

/**
 * @brief DebuggerErrorConsole::DebuggerErrorConsole
 * @param parent
 * @param f
 */
DebuggerErrorConsole::DebuggerErrorConsole(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {
}

}
