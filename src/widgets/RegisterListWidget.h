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

#ifndef REGISTER_LIST_WIDGET_20061222_H_
#define REGISTER_LIST_WIDGET_20061222_H_

#include <QTreeWidget>

class QTreeWidgetItem;
class QString;

class RegisterListWidget : public QTreeWidget {
	Q_OBJECT

public:
	RegisterListWidget(QWidget *parent = 0);
	virtual ~RegisterListWidget();

private Q_SLOTS:
    void handleMousePress(QTreeWidgetItem *item);

public:
	virtual void mouseDoubleClickEvent(QMouseEvent * event);

public:
	QTreeWidgetItem *addCategory(const QString &name);
	bool isCategory(QTreeWidgetItem *item) const;
};

#endif

