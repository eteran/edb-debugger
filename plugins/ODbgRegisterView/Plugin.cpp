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
#include "edb.h"
#include <QDockWidget>
#include <QMainWindow>
#include <QMenu>
#include <QSettings>
#include "RegisterView.h"
#include "ArchProcessor.h"

namespace ODbgRegisterView
{

Plugin::Plugin()
	: QObject(0),
	  menu_(0)
{
}

#define PLUGIN_NAME "ObgRegisterView"
#define DOCK_COUNT "dockCount"

void Plugin::setupDocks(QMainWindow* mainWindow)
{
	const auto dockCount=QSettings().value(PLUGIN_NAME "/" DOCK_COUNT,1).toInt();
	for(int dockN=0;dockN<dockCount;++dockN)
	{
		const auto registerViewDock=createRegisterView();
		if(QDockWidget* registersDock  = mainWindow->findChild<QDockWidget*>("registersDock"))
			mainWindow->tabifyDockWidget(registersDock, registerViewDock);
	}
}

QDockWidget* Plugin::createRegisterView()
{
	if(auto* const mainWindow = qobject_cast<QMainWindow*>(edb::v1::debugger_ui))
	{
		const auto dockN=registerViews_.size();
		const auto regView=new ODBRegView(dockN,mainWindow);
		registerViews_.emplace_back(regView);
		regView->setModel(&edb::v1::arch_processor().get_register_view_model());

		auto* const dock = new QDockWidget(tr("Registers"), mainWindow);
		dock->setObjectName(QString(PLUGIN_NAME"-%1").arg(dockN));
		dock->setWidget(regView);

		mainWindow->addDockWidget(Qt::RightDockWidgetArea, dock);

		return dock;
	}
	return nullptr;
}

QMenu* Plugin::menu(QWidget* parent)
{
	if(!menu_)
	{
		if(auto* const mainWindow = qobject_cast<QMainWindow*>(edb::v1::debugger_ui))
		{
			setupDocks(mainWindow);
			// make the menu and add the show/hide toggle for the widget
			menu_ = new QMenu(PLUGIN_NAME, parent);
			const auto newRegisterView=new QAction(tr("New Register View"),menu_);
			connect(newRegisterView,SIGNAL(triggered()),this,SLOT(createRegisterView()));
			menu_->addAction(newRegisterView);
		}
	}

	return menu_;
}

QList<QAction*> Plugin::cpu_context_menu()
{

	QList<QAction*> ret;

	return ret;
}

#if QT_VERSION < 0x050000
Q_EXPORT_PLUGIN2(ODbgRegisterView, Plugin)
#endif

}
