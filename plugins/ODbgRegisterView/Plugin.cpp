/*
Copyright (C) 2015 Ruslan Kabatsayev <b7.10110111@gmail.com>

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
#include "ArchProcessor.h"
#include "QtHelper.h"
#include "RegisterView.h"
#include "edb.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDockWidget>
#include <QMainWindow>
#include <QMenu>
#include <QSettings>

namespace ODbgRegisterView {
//Q_DECLARE_NAMESPACE_TR(ODbgRegisterView)

namespace {
const auto pluginName             = QLatin1String("ODbgRegisterView");
const auto dockNameSuffixTemplate = QString(" <%1>");
const auto dockObjectNameTemplate = QString(pluginName + "-%1");
const auto views                  = QLatin1String("views");
}

Plugin::Plugin(QObject *parent)
	: QObject(parent) {

	connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, &Plugin::saveSettings);
}

QString Plugin::dockName() const {
	return tr("Registers");
}

void Plugin::setupDocks() {
	QSettings settings;
	settings.beginGroup(pluginName);

	if (settings.value(views + "/size").isValid()) {
		const int size = settings.beginReadArray(views);
		for (int i = 0; i < size; ++i) {
			settings.setArrayIndex(i);
			createRegisterView(settings.group());
		}
	} else {
		createRegisterView();
	}
}

void Plugin::saveSettings() const {
	QSettings settings;
	const int size      = registerViews_.size();
	const auto arrayKey = pluginName + "/" + views;
	settings.remove(arrayKey);
	settings.beginWriteArray(arrayKey, size);
	for (int i = 0; i < size; ++i) {
		settings.setArrayIndex(i);
		registerViews_[i]->saveState(settings.group());
	}
}

void Plugin::createRegisterView() {
	createRegisterView("");
}

void Plugin::createRegisterView(const QString &settingsGroup) {

	if (auto *const mainWindow = qobject_cast<QMainWindow *>(edb::v1::debugger_ui)) {
		const auto regView = new ODBRegView(settingsGroup, mainWindow);
		registerViews_.emplace_back(regView);
		regView->setModel(&edb::v1::arch_processor().registerViewModel());

		const QString suffix          = registerViews_.size() > 1 ? dockNameSuffixTemplate.arg(registerViews_.size()) : "";
		auto *const regViewDockWidget = new QDockWidget(dockName() + suffix, mainWindow);
		const auto viewNumber         = registerViews_.size();
		regViewDockWidget->setObjectName(dockObjectNameTemplate.arg(viewNumber));
		regViewDockWidget->setWidget(regView);

		mainWindow->addDockWidget(Qt::RightDockWidgetArea, regViewDockWidget);

		QList<QDockWidget *> dockWidgets = mainWindow->findChildren<QDockWidget *>();
		for (QDockWidget *widget : dockWidgets) {
			if (widget != regViewDockWidget) {
				if (mainWindow->dockWidgetArea(widget) == Qt::RightDockWidgetArea) {
					mainWindow->tabifyDockWidget(widget, regViewDockWidget);

					// place the new doc widget OVER the one we tabbed with
					// register view is important...
					regViewDockWidget->show();
					regViewDockWidget->raise();
					break;
				}
			}
		}

		Q_ASSERT(menu_);
		const auto removeDockAction = new QAction(tr("Remove %1").arg(regViewDockWidget->windowTitle()), menu_);

		connect(removeDockAction, &QAction::triggered, this, [this, regViewDockWidget]() {
			removeDock(regViewDockWidget);
		});

		menuDeleteRegViewActions_.emplace_back(removeDockAction);
		menu_->addAction(removeDockAction);
	}
}

void Plugin::renumerateDocks() const {
	for (std::size_t i = 0; i < registerViews_.size(); ++i) {
		const auto view = registerViews_[i];
		Q_ASSERT(dynamic_cast<QDockWidget *>(view->parentWidget()));
		QWidget *dock = view->parentWidget();
		dock->setObjectName(dockObjectNameTemplate.arg(i + 1));
		dock->setWindowTitle(dockName() + (i ? dockNameSuffixTemplate.arg(i + 1) : ""));
	}
}

void Plugin::removeDock(QWidget *whatToRemove) {
	Q_ASSERT(dynamic_cast<QDockWidget *>(whatToRemove));
	const auto dockToRemove = static_cast<QDockWidget *>(whatToRemove);

	auto &views(registerViews_);
	const auto viewIter = std::find(views.begin(), views.end(), dockToRemove->widget());

	const auto viewIndex = viewIter - views.begin();
	const auto action    = menuDeleteRegViewActions_[viewIndex];

	whatToRemove->deleteLater();
	action->deleteLater();
	menu_->removeAction(action);
	views.erase(viewIter);
	menuDeleteRegViewActions_.erase(viewIndex + menuDeleteRegViewActions_.begin());

	renumerateDocks();
}

void Plugin::expandLSDown(bool checked) const {
	if (const auto mainWindow = qobject_cast<QMainWindow *>(edb::v1::debugger_ui)) {
		mainWindow->setCorner(Qt::BottomLeftCorner, checked ? Qt::LeftDockWidgetArea : Qt::BottomDockWidgetArea);
	}
}

void Plugin::expandRSDown(bool checked) const {
	if (const auto mainWindow = qobject_cast<QMainWindow *>(edb::v1::debugger_ui)) {
		mainWindow->setCorner(Qt::BottomRightCorner, checked ? Qt::RightDockWidgetArea : Qt::BottomDockWidgetArea);
	}
}

void Plugin::expandLSUp(bool checked) const {
	if (const auto mainWindow = qobject_cast<QMainWindow *>(edb::v1::debugger_ui)) {
		mainWindow->setCorner(Qt::TopLeftCorner, checked ? Qt::LeftDockWidgetArea : Qt::TopDockWidgetArea);
	}
}

void Plugin::expandRSUp(bool checked) const {
	if (const auto mainWindow = qobject_cast<QMainWindow *>(edb::v1::debugger_ui)) {
		mainWindow->setCorner(Qt::TopRightCorner, checked ? Qt::RightDockWidgetArea : Qt::TopDockWidgetArea);
	}
}

QMenu *Plugin::menu(QWidget *parent) {
	if (!menu_) {
		menu_ = new QMenu(tr("OllyDbg-like Register View"), parent);
		{
			const auto newRegisterView = new QAction(tr("New Register View"), menu_);
			connect(newRegisterView, &QAction::triggered, this, [this](bool) {
				createRegisterView();
			});

			menu_->addAction(newRegisterView);
		}
		// FIXME: setChecked calls currently don't really work, since at this stage mainWindow hasn't yet restored its state
		if (auto *const mainWindow = qobject_cast<QMainWindow *>(edb::v1::debugger_ui)) {
			{
				const auto expandLeftSideUp = new QAction(tr("Expand Left-Hand Side Dock Up"), menu_);
				expandLeftSideUp->setCheckable(true);
				expandLeftSideUp->setChecked(mainWindow->corner(Qt::TopLeftCorner) == Qt::LeftDockWidgetArea);
				connect(expandLeftSideUp, &QAction::toggled, this, &Plugin::expandLSUp);
				menu_->addAction(expandLeftSideUp);
			}
			{
				const auto expandLeftSideDown = new QAction(tr("Expand Left-Hand Side Dock Down"), menu_);
				expandLeftSideDown->setCheckable(true);
				expandLeftSideDown->setChecked(mainWindow->corner(Qt::BottomLeftCorner) == Qt::LeftDockWidgetArea);
				connect(expandLeftSideDown, &QAction::toggled, this, &Plugin::expandLSDown);
				menu_->addAction(expandLeftSideDown);
			}
			{
				const auto expandRightSideUp = new QAction(tr("Expand Right-Hand Side Dock Up"), menu_);
				expandRightSideUp->setCheckable(true);
				expandRightSideUp->setChecked(mainWindow->corner(Qt::TopRightCorner) == Qt::RightDockWidgetArea);
				connect(expandRightSideUp, &QAction::toggled, this, &Plugin::expandRSUp);
				menu_->addAction(expandRightSideUp);
			}
			{
				const auto expandRightSideDown = new QAction(tr("Expand Right-Hand Side Dock Down"), menu_);
				expandRightSideDown->setCheckable(true);
				expandRightSideDown->setChecked(mainWindow->corner(Qt::BottomRightCorner) == Qt::RightDockWidgetArea);
				connect(expandRightSideDown, &QAction::toggled, this, &Plugin::expandRSDown);
				menu_->addAction(expandRightSideDown);
			}
			menu_->addSeparator();
		}

		setupDocks();
	}

	return menu_;
}

}
