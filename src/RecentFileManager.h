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

#ifndef RECENTFILEMANAGER_20070430_H_
#define RECENTFILEMANAGER_20070430_H_

#include <QStringList>
#include <QWidget>
#include <QByteArray>
#include <QMetaType>
#include <utility>

class QMenu;
class QWidget;
class QString;

class RecentFileManager : public QWidget {
	Q_OBJECT
public:
	using RecentFile=std::pair<QString,QList<QByteArray>>;
public:
	RecentFileManager(QWidget * parent = 0, Qt::WindowFlags f = 0);
	virtual ~RecentFileManager();

public:
	void add_file(const QString &file, const QList<QByteArray> &args);
	QMenu *create_menu();
	RecentFile most_recent() const;
	int entry_count() const;

public Q_SLOTS:
	void clear_file_list();
	void item_selected();

Q_SIGNALS:
	void file_selected(const QString &,const QList<QByteArray>&);

private:
	void update();
	static QString format_entry(RecentFile const& file);

private:
	QList<RecentFile> file_list_;
	QMenu *     menu_;
};

Q_DECLARE_METATYPE(RecentFileManager::RecentFile)

#endif

