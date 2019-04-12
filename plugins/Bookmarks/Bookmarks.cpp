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
#include <QtDebug>

namespace BookmarksPlugin {

//------------------------------------------------------------------------------
// Name: Bookmarks
// Desc:
//------------------------------------------------------------------------------
Bookmarks::Bookmarks(QObject *parent) : QObject(parent) {
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


			QList<QDockWidget *> dockWidgets = main_window->findChildren<QDockWidget *>();
			for(QDockWidget *widget : dockWidgets) {
				if(widget != dock_widget) {
					if(main_window->dockWidgetArea(widget) == Qt::RightDockWidgetArea) {
						main_window->tabifyDockWidget(widget, dock_widget);

						// place the new doc widget UNDER the one we tabbed with
						widget->show();
						widget->raise();
						break;
					}
				}
			}


			// make the menu and add the show/hide toggle for the widget
			menu_ = new QMenu(tr("Bookmarks"), parent);
			menu_->addAction(dock_widget->toggleViewAction());

			for(int i = 0; i < 10; ++i) {
				// create an action and attach it to the signal mapper
				auto action = new QShortcut(QKeySequence(tr("Ctrl+%1").arg(i)), main_window);
				connect(action, &QShortcut::activated, this, [this, index = (i == 0) ? 9 : (i - 1)]() {
					bookmark_widget_->shortcut(index);
				});
			}
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
	connect(action_bookmark, &QAction::triggered, this, &Bookmarks::add_bookmark_menu);
	ret << action_bookmark;

	return ret;
}

//------------------------------------------------------------------------------
// Name: add_bookmark_menu
// Desc:
//------------------------------------------------------------------------------
void Bookmarks::add_bookmark_menu() {
	bookmark_widget_->add_address(edb::v1::cpu_selected_address());
}

//------------------------------------------------------------------------------
// Name: save_state
// Desc:
//------------------------------------------------------------------------------
QVariantMap Bookmarks::save_state() const {
	QVariantMap  state;
	QVariantList bookmarks;
	for(auto &bookmark : bookmark_widget_->entries()) {
	
		QVariantMap entry;
		entry["address"] = bookmark.address.toHexString();
		entry["type"]    = BookmarksModel::BookmarkTypeToString(bookmark.type);
		entry["comment"] = bookmark.comment;
	
		bookmarks.push_back(entry);
	}

	state["bookmarks"] = bookmarks;
	return state;
}

//------------------------------------------------------------------------------
// Name: restore_state
// Desc:
//------------------------------------------------------------------------------
void Bookmarks::restore_state(const QVariantMap &state) {

	QVariantList bookmarks = state["bookmarks"].toList();
	for(auto &entry : bookmarks) {
		auto bookmark = entry.value<QVariantMap>();
	
		edb::address_t address = edb::address_t::fromHexString(bookmark["address"].toString());
		QString type           = bookmark["type"].toString();
		QString comment        = bookmark["comment"].toString();
		
		qDebug() << "Restoring bookmark with address: " << address.toHexString();
		
		bookmark_widget_->add_address(address, type, comment);
	}

}

}
