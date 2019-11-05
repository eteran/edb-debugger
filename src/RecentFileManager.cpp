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

#include <QAction>
#include <QFileInfo>
#include <QMenu>
#include <QSettings>

#include <algorithm>

namespace {
constexpr int MaxRecentFiles = 8;
}

//------------------------------------------------------------------------------
// Name: RecentFileManager
// Desc: constructor
//------------------------------------------------------------------------------
RecentFileManager::RecentFileManager(QWidget *parent, Qt::WindowFlags f)
	: QWidget(parent, f) {

	QSettings settings;
	settings.beginGroup("Recent");
	const int size = settings.beginReadArray("recent.files");

	for (int i = 0; i < size; ++i) {
		settings.setArrayIndex(i);
		const auto file = settings.value("file").toString();

		if (file.isEmpty()) {
			continue;
		}

		const int size = settings.beginReadArray("arguments");

		QList<QByteArray> args;
		for (int i = 0; i < size; ++i) {
			settings.setArrayIndex(i);
			args.push_back(settings.value("arg").toByteArray());
		}

		settings.endArray();
		files_.push_back(std::make_pair(file, args));
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

	for (int i = 0; i < files_.size(); ++i) {
		const auto &file = files_[i];
		settings.setArrayIndex(i);
		settings.setValue("file", QVariant::fromValue(file.first));
		settings.beginWriteArray("arguments");

		for (int i = 0; i < file.second.size(); ++i) {
			settings.setArrayIndex(i);
			settings.setValue("arg", file.second[i]);
		}
		settings.endArray();
	}

	settings.endArray();
	settings.endGroup();
}

//------------------------------------------------------------------------------
// Name: clear
// Desc:
//------------------------------------------------------------------------------
void RecentFileManager::clear() {
	files_.clear();
	if (menu_) {
		menu_->clear();
		menu_->addSeparator();
		menu_->addAction(tr("Clear &Menu"), this, SLOT(clear()));
	}
}

//------------------------------------------------------------------------------
// Name: createMenu
// Desc:
//------------------------------------------------------------------------------
QMenu *RecentFileManager::createMenu() {

	if (!menu_) {
		menu_ = new QMenu(this);
		update();
	}

	return menu_;
}

//------------------------------------------------------------------------------
// Name: formatEntry
// Desc:
//------------------------------------------------------------------------------
QString RecentFileManager::formatEntry(const RecentFile &file) {
	QString str = file.first;
	for (const auto &arg : file.second) {
		str += " " + QString(arg);
	}
	return str;
}

//------------------------------------------------------------------------------
// Name: update
// Desc:
//------------------------------------------------------------------------------
void RecentFileManager::update() {
	if (menu_) {
		menu_->clear();

		Q_FOREACH (const auto &file, files_) {
			if (QAction *const action = menu_->addAction(formatEntry(file), this, SLOT(itemSelected()))) {
				action->setData(QVariant::fromValue(file));
			}
		}

		menu_->addSeparator();
		menu_->addAction(tr("Clear &Menu"), this, SLOT(clear()));
	}
}

//------------------------------------------------------------------------------
// Name: mostRecent
// Desc:
//------------------------------------------------------------------------------
RecentFileManager::RecentFile RecentFileManager::mostRecent() const {
	if (files_.isEmpty()) return {};
	return files_.front();
}

//------------------------------------------------------------------------------
// Name: entryCount
// Desc:
//------------------------------------------------------------------------------
int RecentFileManager::entryCount() const {
	return files_.size();
}

//------------------------------------------------------------------------------
// Name: itemSelected
// Desc:
//------------------------------------------------------------------------------
void RecentFileManager::itemSelected() {
	if (auto action = qobject_cast<QAction *>(sender())) {
		const auto file = action->data().value<RecentFile>();
		Q_EMIT fileSelected(file.first, file.second);
	}
}

//------------------------------------------------------------------------------
// Name: addFile
// Desc:
//------------------------------------------------------------------------------
void RecentFileManager::addFile(const QString &file, const QList<QByteArray> &args) {

	QFileInfo fi(file);
	QString path = fi.absoluteFilePath();

	// update recent file list, we remove all entries for this file (if any)
	// and then push the file on the front, ensuring that the recently run
	// entries are higher in the list
	files_.erase(std::remove_if(files_.begin(), files_.end(), [&path](const RecentFile &file) {
					 return file.first == path;
				 }),
				 files_.end());

	files_.push_front(std::make_pair(path, args));

	// make sure we don't add more than the max
	while (files_.size() > MaxRecentFiles) {
		files_.pop_back();
	}
	update();
}
