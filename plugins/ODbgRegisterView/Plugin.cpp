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
#include <QDebug>

namespace ODbgRegisterView
{

Plugin::Plugin()
	: QObject(0),
	  menu_(0)
{
	connect(QCoreApplication::instance(),SIGNAL(aboutToQuit()),this,SLOT(saveState()));
}

const QString pluginName="ODbgRegisterView";
const QString VIEW="views";

void Plugin::setupDocks()
{
	QSettings settings;
	settings.beginGroup(pluginName);
	if(settings.value(VIEW+"/size").isValid())
	{
		const int size=settings.beginReadArray(VIEW);
		for(int i=0;i<size;++i)
		{
			settings.setArrayIndex(i);
			createRegisterView(settings.group());
		}
	}
	else createRegisterView();
}

void Plugin::saveState() const
{
	QSettings settings;
	const int size=registerViews_.size();
	const auto arrayKey=pluginName+"/"+VIEW;
	settings.remove(arrayKey);
	settings.beginWriteArray(arrayKey,size);
	for(int i=0;i<size;++i)
	{
		settings.setArrayIndex(i);
		registerViews_[i]->saveState(settings.group());
	}
}

void Plugin::createRegisterView()
{
	createRegisterView("");
}

void Plugin::createRegisterView(QString const& settingsGroup)
{
	if(auto* const mainWindow = qobject_cast<QMainWindow*>(edb::v1::debugger_ui))
	{
		const auto regView=new ODBRegView(settingsGroup,mainWindow);
		registerViews_.emplace_back(regView);
		regView->setModel(&edb::v1::arch_processor().get_register_view_model());

		auto* const regViewDockWidget = new QDockWidget(tr("Registers"), mainWindow);
		const auto viewNumber=registerViews_.size();
		regViewDockWidget->setObjectName(QString(pluginName+"-%1").arg(viewNumber));
		regViewDockWidget->setWidget(regView);

		mainWindow->addDockWidget(Qt::RightDockWidgetArea, regViewDockWidget);

		if(QDockWidget* registersDock  = mainWindow->findChild<QDockWidget*>("registersDock"))
			mainWindow->tabifyDockWidget(registersDock, regViewDockWidget);

	}
}

void Plugin::expandLSDown(bool checked) const
{
	if(auto* const mainWindow = qobject_cast<QMainWindow*>(edb::v1::debugger_ui))
		mainWindow->setCorner(Qt::BottomLeftCorner,checked ? Qt::LeftDockWidgetArea : Qt::BottomDockWidgetArea);
}

void Plugin::expandRSDown(bool checked) const
{
	if(auto* const mainWindow = qobject_cast<QMainWindow*>(edb::v1::debugger_ui))
		mainWindow->setCorner(Qt::BottomRightCorner,checked ? Qt::RightDockWidgetArea : Qt::BottomDockWidgetArea);
}

void Plugin::expandLSUp(bool checked) const
{
	if(auto* const mainWindow = qobject_cast<QMainWindow*>(edb::v1::debugger_ui))
		mainWindow->setCorner(Qt::TopLeftCorner,checked ? Qt::LeftDockWidgetArea : Qt::TopDockWidgetArea);
}

void Plugin::expandRSUp(bool checked) const
{
	if(auto* const mainWindow = qobject_cast<QMainWindow*>(edb::v1::debugger_ui))
		mainWindow->setCorner(Qt::TopRightCorner,checked ? Qt::RightDockWidgetArea : Qt::TopDockWidgetArea);
}

QMenu* Plugin::menu(QWidget* parent)
{
	if(!menu_)
	{
		setupDocks();

		menu_ = new QMenu("OllyDbg-like Register View", parent);
		{
			const auto newRegisterView=new QAction(tr("New Register View"),menu_);
			connect(newRegisterView,SIGNAL(triggered()),this,SLOT(createRegisterView()));
			menu_->addAction(newRegisterView);
		}
		// FIXME: setChecked calls currently don't really work, since at this stage mainWindow hasn't yet restored its state
		if(auto* const mainWindow = qobject_cast<QMainWindow*>(edb::v1::debugger_ui))
		{
			{
				const auto expandLeftSideUp=new QAction(tr("Expand Left-Hand Side Dock Up"),menu_);
				expandLeftSideUp->setCheckable(true);
				expandLeftSideUp->setChecked(mainWindow->corner(Qt::TopLeftCorner)==Qt::LeftDockWidgetArea);
				connect(expandLeftSideUp,SIGNAL(toggled(bool)),this,SLOT(expandLSUp(bool)));
				menu_->addAction(expandLeftSideUp);
			}
			{
				const auto expandLeftSideDown=new QAction(tr("Expand Left-Hand Side Dock Down"),menu_);
				expandLeftSideDown->setCheckable(true);
				expandLeftSideDown->setChecked(mainWindow->corner(Qt::BottomLeftCorner)==Qt::LeftDockWidgetArea);
				connect(expandLeftSideDown,SIGNAL(toggled(bool)),this,SLOT(expandLSDown(bool)));
				menu_->addAction(expandLeftSideDown);
			}
			{
				const auto expandRightSideUp=new QAction(tr("Expand Right-Hand Side Dock Up"),menu_);
				expandRightSideUp->setCheckable(true);
				expandRightSideUp->setChecked(mainWindow->corner(Qt::TopRightCorner)==Qt::RightDockWidgetArea);
				connect(expandRightSideUp,SIGNAL(toggled(bool)),this,SLOT(expandRSUp(bool)));
				menu_->addAction(expandRightSideUp);
			}
			{
				const auto expandRightSideDown=new QAction(tr("Expand Right-Hand Side Dock Down"),menu_);
				expandRightSideDown->setCheckable(true);
				expandRightSideDown->setChecked(mainWindow->corner(Qt::BottomRightCorner)==Qt::RightDockWidgetArea);
				connect(expandRightSideDown,SIGNAL(toggled(bool)),this,SLOT(expandRSDown(bool)));
				menu_->addAction(expandRightSideDown);
			}
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
