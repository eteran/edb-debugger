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

#include "RecentFileManager.h"

#include <QSettings>
#include <QAction>
#include <QMenu>
#include <QFileInfo>

namespace {
const int MaxRecentFiles = 8;
}

//------------------------------------------------------------------------------
// Name: RecentFileManager
// Desc: constructor
//------------------------------------------------------------------------------
RecentFileManager::RecentFileManager(QWidget *parent, Qt::WindowFlags f) : QWidget(parent, f), menu_(0) {
	QSettings settings;
	settings.beginGroup("Recent");
	file_list_ = settings.value("recent.files", QStringList()).value<QStringList>();
	settings.endGroup();
}

//------------------------------------------------------------------------------
// Name: ~RecentFileManager
// Desc: destructor
//------------------------------------------------------------------------------
RecentFileManager::~RecentFileManager() {
	QSettings settings;
	settings.beginGroup("Recent");
	settings.setValue("recent.files", file_list_);
	settings.endGroup();
}


//------------------------------------------------------------------------------
// Name: clear_file_list
// Desc:
//------------------------------------------------------------------------------
void RecentFileManager::clear_file_list() {
	file_list_.clear();
	if(menu_) {
		menu_->clear();
		menu_->addSeparator();
		menu_->addAction(tr("Clear &Menu"), this, SLOT(clear_file_list()));
	}
}

//------------------------------------------------------------------------------
// Name: create_menu
// Desc:
//------------------------------------------------------------------------------
QMenu *RecentFileManager::create_menu() {

	if(!menu_) {
		menu_ = new QMenu(this);
		update();
	}

	return menu_;
}

//------------------------------------------------------------------------------
// Name: create_menu
// Desc:
//------------------------------------------------------------------------------
void RecentFileManager::update() {
	if(menu_) {
		menu_->clear();

		for(const QString &s: file_list_) {
			if(QAction *const action = menu_->addAction(s, this, SLOT(item_selected()))) {
				action->setData(s);
			}
		}

		menu_->addSeparator();
		menu_->addAction(tr("Clear &Menu"), this, SLOT(clear_file_list()));
	}
}

//------------------------------------------------------------------------------
// Name: item_selected
// Desc:
//------------------------------------------------------------------------------
void RecentFileManager::item_selected() {
	if(auto action = qobject_cast<QAction *>(sender())) {
		const QString s = action->data().toString();
		Q_EMIT file_selected(s);
	}
}

//------------------------------------------------------------------------------
// Name: add_file
// Desc:
//------------------------------------------------------------------------------
void RecentFileManager::add_file(const QString &file) {

	QFileInfo fi(file);
	QString path = fi.absoluteFilePath();
	
	// update recent file list, we remove all entries for this file (if any)
	// and then push the file on the front, ensuring that the recently run 
	// entries are higher in the list
	file_list_.removeAll(path);
	file_list_.push_front(path);

	// make sure we don't add more than the max
	while(file_list_.size() > MaxRecentFiles) {
		file_list_.pop_back();
	}
	update();

}
