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

#ifndef RECENT_FILE_MANAGER_H_20070430_
#define RECENT_FILE_MANAGER_H_20070430_

#include <QByteArray>
#include <QMetaType>
#include <QWidget>
#include <utility>

class QMenu;
class QWidget;
class QString;

class RecentFileManager : public QWidget {
	Q_OBJECT
public:
	using RecentFile = std::pair<QString, QList<QByteArray>>;

public:
	explicit RecentFileManager(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~RecentFileManager() override;

public:
	void addFile(const QString &file, const QList<QByteArray> &args);
	QMenu *createMenu();
	RecentFile mostRecent() const;
	int entryCount() const;

public Q_SLOTS:
	void clear();
	void itemSelected();

Q_SIGNALS:
	void fileSelected(const QString &, const QList<QByteArray> &);

private:
	void update();
	static QString formatEntry(const RecentFile &file);

private:
	QList<RecentFile> files_;
	QMenu *menu_ = nullptr;
};

Q_DECLARE_METATYPE(RecentFileManager::RecentFile)

#endif
