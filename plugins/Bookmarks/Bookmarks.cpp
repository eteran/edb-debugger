/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "Bookmarks.h"
#include "BookmarkWidget.h"
#include "IDebugger.h"
#include "IProcess.h"
#include "Module.h"
#include "edb.h"

#include <QDockWidget>
#include <QMainWindow>
#include <QMenu>
#include <QShortcut>
#include <QtDebug>

namespace BookmarksPlugin {

/**
 * @brief Constructs the Bookmarks plugin object.
 *
 * @param parent
 */
Bookmarks::Bookmarks(QObject *parent)
	: QObject(parent) {
}

/**
 * @brief Creates the Bookmarks dock widget, registers Ctrl+0–9 shortcuts, and builds the menu on first call.
 *
 * @param parent
 * @return
 */
QMenu *Bookmarks::menu(QWidget *parent) {

	Q_ASSERT(parent);

	if (!menu_) {

		// if we are dealing with a main window (and we are...)
		// add the dock object
		if (const auto main_window = edb::v1::debugger_ui->findChild<QMainWindow *>(QStringLiteral("dockingRoot"))) {
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
 * @brief Returns the CPU context menu actions contributed by the Bookmarks plugin.
 *
 * @return
 */
QList<QAction *> Bookmarks::cpuContextMenu() {

	auto action_bookmark = new QAction(tr("Add &Bookmark"), this);
	connect(action_bookmark, &QAction::triggered, this, &Bookmarks::addBookmarkMenu);
	return {action_bookmark};
}

/**
 * @brief Adds the currently selected CPU address as a bookmark.
 */
void Bookmarks::addBookmarkMenu() {
	bookmarkWidget_->addAddress(edb::v1::cpu_selected_address());
}

/**
 * @brief Serializes the current bookmark list to a QVariantMap for session state persistence.
 *
 * @return
 */
QVariantMap Bookmarks::saveState() const {
	QVariantMap state;
	QVariantList bookmarks;

	for (auto &bookmark : bookmarkWidget_->entries()) {

		QVariantMap entry;
		entry[QStringLiteral("type")]    = BookmarksModel::bookmarkTypeToString(bookmark.type);
		entry[QStringLiteral("comment")] = bookmark.comment;
		entry[QStringLiteral("module")]  = bookmark.module ? bookmark.module->name : QString();
		entry[QStringLiteral("offset")]  = (bookmark.module) ? (bookmark.address - bookmark.module->baseAddress).toHexString() : QString();

		bookmarks.push_back(entry);
	}

	state[QStringLiteral("bookmarks")] = bookmarks;
	return state;
}

/**
 * @brief Restores the bookmark list from a previously serialized QVariantMap session state.
 *
 * @param state
 */
void Bookmarks::restoreState(const QVariantMap &state) {

	IProcess *process = edb::v1::debugger_core->process();
	Q_ASSERT(process);

	QSet<Module> modules = process->loadedModules();

	QVariantList bookmarks = state[QStringLiteral("bookmarks")].toList();
	for (auto &entry : bookmarks) {
		auto bookmark = entry.value<QVariantMap>();

		QString module_name = bookmark[QStringLiteral("module")].toString();
		QString offset_str  = bookmark[QStringLiteral("offset")].toString();
		QString type        = bookmark[QStringLiteral("type")].toString();
		QString comment     = bookmark[QStringLiteral("comment")].toString();

		edb::address_t offset = edb::address_t::fromHexString(offset_str);

		// Figure out which module this bookmark belongs to and add it if the module is loaded
		auto it = std::find_if(modules.begin(), modules.end(), [&module_name](const Module &module) {
			return edb::v2::compare_module_names(module.name, module_name);
		});

		if (it != modules.end()) {
			edb::address_t address = offset + it->baseAddress;
			bookmarkWidget_->addAddress(address, type, comment);
			continue;
		} else {

			// If the module is not loaded, store the bookmark entry for later restoration when the module is loaded
			BookmarkEntry entry;
			entry.type    = type;
			entry.comment = comment;
			entry.module  = module_name;
			entry.offset  = offset_str;
			deferredBookmarks_.push_back(entry);
		}
	}
}

/**
 * @brief Handles library load/unload events to restore bookmarks for newly loaded modules.
 *
 * @param module The module that was loaded or unloaded.
 * @param loaded True if the module was loaded, false if it was unloaded.
 */
void Bookmarks::libraryEvent(const Module &module, bool loaded) {
	if (loaded) {
		auto it = std::remove_if(deferredBookmarks_.begin(), deferredBookmarks_.end(), [&module, this](const BookmarkEntry &entry) {
			if (edb::v2::compare_module_names(entry.module, module.name)) {
				edb::address_t offset  = edb::address_t::fromHexString(entry.offset);
				edb::address_t address = offset + module.baseAddress;
				bookmarkWidget_->addAddress(address, entry.type, entry.comment);
				return true;
			}

			return false;
		});
		deferredBookmarks_.erase(it, deferredBookmarks_.end());
	}
}

}
