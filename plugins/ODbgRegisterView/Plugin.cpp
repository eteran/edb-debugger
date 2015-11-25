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
#include "RegisterView.h"
#include "ArchProcessor.h"

namespace ODbgRegisterView
{

Plugin::Plugin()
	: QObject(0),
	  menu_(0),
	  registerView_(0)
{
}

QMenu* Plugin::menu(QWidget* parent)
{
	if(!menu_)
	{
		if(auto* const mainWindow = qobject_cast<QMainWindow*>(edb::v1::debugger_ui))
		{
			registerView_ = new ODBRegView(mainWindow);
			registerView_->setModel(&edb::v1::arch_processor().get_register_view_model());

			auto* const dock = new QDockWidget(tr("Registers"), mainWindow);
			dock->setObjectName(QString::fromUtf8("ODbgRegisterView"));
			dock->setWidget(registerView_);

			mainWindow->addDockWidget(Qt::RightDockWidgetArea, dock);
			
			if(QDockWidget* registersDock  = mainWindow->findChild<QDockWidget*>("registersDock"))
				mainWindow->tabifyDockWidget(registersDock, dock);

			// make the menu and add the show/hide toggle for the widget
			menu_ = new QMenu(tr("Plugin"), parent);
			menu_->addAction(dock->toggleViewAction());
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
