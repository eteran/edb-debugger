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
#include <algorithm>

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
	const auto size=settings.beginReadArray("recent.files");
	for(int i=0;i<size;++i)
	{
			settings.setArrayIndex(i);
		const auto file=settings.value("file").toString();
		if(file.isEmpty()) continue;
		const auto size=settings.beginReadArray("arguments");
		QList<QByteArray> args;
		for(int i=0;i<size;++i)
		{
			settings.setArrayIndex(i);
			args.push_back(settings.value("arg").toByteArray());
		}
		settings.endArray();
		file_list_.push_back(std::make_pair(file,args));
	}
	settings.endArray();
	settings.endGroup();
}

//------------------------------------------------------------------------------
// Name: ~RecentFileManager
// Desc: destructor
//------------------------------------------------------------------------------
RecentFileManager::~RecentFileManager() {
	QSettings settings;
	settings.beginGroup("Recent");
		settings.beginWriteArray("recent.files");
		for(int i=0;i<file_list_.size();++i)
		{
			const auto& file=file_list_[i];
			settings.setArrayIndex(i);
			settings.setValue("file",      QVariant::fromValue(file.first ));
			settings.beginWriteArray("arguments");
			for(int i=0;i<file.second.size();++i)
			{
				settings.setArrayIndex(i);
				settings.setValue("arg",file.second[i]);
			}
			settings.endArray();
		}
		settings.endArray();
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
// Name: format_entry
// Desc:
//------------------------------------------------------------------------------
QString RecentFileManager::format_entry(RecentFile const& file) {
	QString str=file.first;
	for(const auto& arg : file.second)
		str += " "+QString(arg);
	return str;
}

//------------------------------------------------------------------------------
// Name: create_menu
// Desc:
//------------------------------------------------------------------------------
void RecentFileManager::update() {
	if(menu_) {
		menu_->clear();

		for(const auto &file: file_list_) {
			if(QAction *const action = menu_->addAction(format_entry(file), this, SLOT(item_selected()))) {
				action->setData(QVariant::fromValue(file));
			}
		}

		menu_->addSeparator();
		menu_->addAction(tr("Clear &Menu"), this, SLOT(clear_file_list()));
	}
}

//------------------------------------------------------------------------------
// Name: most_recent
// Desc:
//------------------------------------------------------------------------------
RecentFileManager::RecentFile RecentFileManager::most_recent() const {
	if(file_list_.isEmpty()) return {};
	return file_list_.front();
}

//------------------------------------------------------------------------------
// Name: entry_count
// Desc:
//------------------------------------------------------------------------------
int RecentFileManager::entry_count() const {
	return file_list_.size();
}

//------------------------------------------------------------------------------
// Name: item_selected
// Desc:
//------------------------------------------------------------------------------
void RecentFileManager::item_selected() {
	if(auto action = qobject_cast<QAction *>(sender())) {
		const auto file = action->data().value<RecentFile>();
		Q_EMIT file_selected(file.first,file.second);
	}
}

//------------------------------------------------------------------------------
// Name: add_file
// Desc:
//------------------------------------------------------------------------------
void RecentFileManager::add_file(const QString &file, const QList<QByteArray> &args) {

	QFileInfo fi(file);
	QString path = fi.absoluteFilePath();
	
	// update recent file list, we remove all entries for this file (if any)
	// and then push the file on the front, ensuring that the recently run 
	// entries are higher in the list
	file_list_.erase(std::remove_if(file_list_.begin(),file_list_.end(),
				[&path](RecentFile const& file){return file.first==path;}),file_list_.end());
	file_list_.push_front(std::make_pair(path,args));

	// make sure we don't add more than the max
	while(file_list_.size() > MaxRecentFiles) {
		file_list_.pop_back();
	}
	update();

}
