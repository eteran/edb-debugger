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

#ifndef BOOKMARKWIDGET_20101207_H_
#define BOOKMARKWIDGET_20101207_H_

#include "Types.h"
#include "ui_Bookmarks.h"
#include "BookmarksModel.h"
#include <QWidget>

class QModelIndex;

namespace BookmarksPlugin {

class BookmarksModel;

class BookmarkWidget : public QWidget {
	Q_OBJECT

public:
	BookmarkWidget(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~BookmarkWidget() override = default;

public Q_SLOTS:
	void on_btnAdd_clicked();
	void on_btnDel_clicked();
	void on_btnClear_clicked();
	void on_tableView_doubleClicked(const QModelIndex &index);
	void on_tableView_customContextMenuRequested(const QPoint &pos);

public:
	void shortcut(int index);
	void add_address(edb::address_t address, const QString &type = QString(), const QString &comment = QString());
	QList<BookmarksModel::Bookmark> entries() const;

private:
	Ui::Bookmarks ui;
	BookmarksModel *model_;
};

}

#endif
