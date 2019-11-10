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

/**
 * @brief Bookmarks::Bookmarks
 * @param parent
 */
Bookmarks::Bookmarks(QObject *parent)
	: QObject(parent) {
}

/**
 * @brief Bookmarks::menu
 * @param parent
 * @return
 */
QMenu *Bookmarks::menu(QWidget *parent) {

	Q_ASSERT(parent);

	if (!menu_) {

		// if we are dealing with a main window (and we are...)
		// add the dock object
		if (auto main_window = qobject_cast<QMainWindow *>(edb::v1::debugger_ui)) {
			bookmarkWidget_ = new BookmarkWidget;

			// make the dock widget and _name_ it, it is important to name it so
			// that it's state is saved in the GUI info
			auto dock_widget = new QDockWidget(tr("Bookmarks"), main_window);
			dock_widget->setObjectName(QString::fromUtf8("Bookmarks"));
			dock_widget->setWidget(bookmarkWidget_);

			// add it to the dock
			main_window->addDockWidget(Qt::RightDockWidgetArea, dock_widget);

			QList<QDockWidget *> dockWidgets = main_window->findChildren<QDockWidget *>();
			for (QDockWidget *widget : dockWidgets) {
				if (widget != dock_widget) {
					if (main_window->dockWidgetArea(widget) == Qt::RightDockWidgetArea) {
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

			for (int i = 0; i < 10; ++i) {
				// create an action and attach it to the signal mapper
				auto action = new QShortcut(QKeySequence(tr("Ctrl+%1").arg(i)), main_window);
				connect(action, &QShortcut::activated, this, [this, index = (i == 0) ? 9 : (i - 1)]() {
					bookmarkWidget_->shortcut(index);
				});
			}
		}
	}

	return menu_;
}

/**
 * @brief Bookmarks::cpuContextMenu
 * @return
 */
QList<QAction *> Bookmarks::cpuContextMenu() {

	QList<QAction *> ret;

	auto action_bookmark = new QAction(tr("Add &Bookmark"), this);
	connect(action_bookmark, &QAction::triggered, this, &Bookmarks::addBookmarkMenu);
	ret << action_bookmark;

	return ret;
}

/**
 * @brief Bookmarks::addBookmarkMenu
 */
void Bookmarks::addBookmarkMenu() {
	bookmarkWidget_->addAddress(edb::v1::cpu_selected_address());
}

/**
 * @brief Bookmarks::saveState
 * @return
 */
QVariantMap Bookmarks::saveState() const {
	QVariantMap state;
	QVariantList bookmarks;
	for (auto &bookmark : bookmarkWidget_->entries()) {

		QVariantMap entry;
		entry["address"] = bookmark.address.toHexString();
		entry["type"]    = BookmarksModel::bookmarkTypeToString(bookmark.type);
		entry["comment"] = bookmark.comment;

		bookmarks.push_back(entry);
	}

	state["bookmarks"] = bookmarks;
	return state;
}

/**
 * @brief Bookmarks::restoreState
 * @param state
 */
void Bookmarks::restoreState(const QVariantMap &state) {

	QVariantList bookmarks = state["bookmarks"].toList();
	for (auto &entry : bookmarks) {
		auto bookmark = entry.value<QVariantMap>();

		edb::address_t address = edb::address_t::fromHexString(bookmark["address"].toString());
		QString type           = bookmark["type"].toString();
		QString comment        = bookmark["comment"].toString();

		qDebug() << "Restoring bookmark with address: " << address.toHexString();

		bookmarkWidget_->addAddress(address, type, comment);
	}
}

}
