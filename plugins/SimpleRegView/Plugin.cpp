/*
Copyright (C) 2017 - 2017 Ruslan Kabatsayev
                          b7.10110111@gmail.com

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
#include "RegView.h"
#include "ArchProcessor.h"
#include "edb.h"
#include <QMainWindow>
#include <QDockWidget>

namespace SimpleRegView
{

Plugin::Plugin() :
	QObject(0),
	regView(0)
{
}

QMenu* Plugin::menu(QWidget*)
{
	if(regView) return nullptr;

	if(auto*const mainWindow = qobject_cast<QMainWindow*>(edb::v1::debugger_ui))
	{
		auto*const regViewDockWidget = new QDockWidget("SimpleRegView", mainWindow);
		regView=new RegView;
		regViewDockWidget->setWidget(regView);
		regViewDockWidget->setObjectName("SimpleRegView");
		regView->setModel(&edb::v1::arch_processor().get_register_view_model());

		const auto dockWidgets = mainWindow->findChildren<QDockWidget*>();
		for(QDockWidget *widget : dockWidgets)
		{
			if(widget == regViewDockWidget)
				continue;
			mainWindow->tabifyDockWidget(widget, regViewDockWidget);
			break;
		}
		regView->show();
	}

	return nullptr;
}

}
