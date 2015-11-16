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

#include "Bookmarks.h"
#include "BookmarkWidget.h"
#include "edb.h"
#include <QDockWidget>
#include <QMainWindow>
#include <QMenu>
#include <QShortcut>
#include <QSignalMapper>

namespace Bookmarks {

//------------------------------------------------------------------------------
// Name: Bookmarks
// Desc:
//------------------------------------------------------------------------------
Bookmarks::Bookmarks() : QObject(0), menu_(0), signal_mapper_(0), bookmark_widget_(0) {
}

//------------------------------------------------------------------------------
// Name: menu
// Desc:
//------------------------------------------------------------------------------
QMenu *Bookmarks::menu(QWidget *parent) {
	
	Q_ASSERT(parent);
	
	if(!menu_) {

		// if we are dealing with a main window (and we are...)
		// add the dock object
		if(auto main_window = qobject_cast<QMainWindow *>(edb::v1::debugger_ui)) {
			bookmark_widget_ = new BookmarkWidget;

			// make the dock widget and _name_ it, it is important to name it so
			// that it's state is saved in the GUI info
			auto dock_widget = new QDockWidget(tr("Bookmarks"), main_window);
			dock_widget->setObjectName(QString::fromUtf8("Bookmarks"));
			dock_widget->setWidget(bookmark_widget_);

			// add it to the dock
			main_window->addDockWidget(Qt::RightDockWidgetArea, dock_widget);
			
			if(QDockWidget *registersDock  = main_window->findChild<QDockWidget *>("registersDock")) {			
				main_window->tabifyDockWidget(registersDock, dock_widget);
				registersDock->show();
				registersDock->raise();				
			}

			// make the menu and add the show/hide toggle for the widget
			menu_ = new QMenu(tr("Bookmarks"), parent);
			menu_->addAction(dock_widget->toggleViewAction());

			signal_mapper_ = new QSignalMapper(this);

			for(int i = 0; i < 10; ++i) {
				// create an action and attach it to the signal mapper
				auto action = new QShortcut(QKeySequence(tr("Ctrl+%1").arg(i)), main_window);
				signal_mapper_->setMapping(action, (i == 0) ? 9 : (i - 1));
				connect(action, SIGNAL(activated()), signal_mapper_, SLOT(map()));
			}

			// connect the parametized signal to the slot..phew finally
			connect(signal_mapper_, SIGNAL(mapped(int)), bookmark_widget_, SLOT(shortcut(int)));
		}
	}

	return menu_;
}

//------------------------------------------------------------------------------
// Name: cpu_context_menu
// Desc:
//------------------------------------------------------------------------------
QList<QAction *> Bookmarks::cpu_context_menu() {

	QList<QAction *> ret;

	auto action_bookmark = new QAction(tr("Add &Bookmark"), this);
	connect(action_bookmark, SIGNAL(triggered()), this, SLOT(add_bookmark_menu()));
	ret << action_bookmark;

	return ret;
}

//------------------------------------------------------------------------------
// Name: addresses
// Desc:
//------------------------------------------------------------------------------
QVariantList Bookmarks::addresses() const {
	QVariantList r;
	QList<edb::address_t> a = bookmark_widget_->entries();
	for(edb::address_t x: a) {
		r.push_back(x);
	}
	return r;
}

//------------------------------------------------------------------------------
// Name: add_bookmark_menu
// Desc:
//------------------------------------------------------------------------------
void Bookmarks::add_bookmark_menu() {
	bookmark_widget_->add_address(edb::v1::cpu_selected_address());
}

#if QT_VERSION < 0x050000
Q_EXPORT_PLUGIN2(Bookmarks, Bookmarks)
#endif

}
