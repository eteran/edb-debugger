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

#include "RegisterListWidget.h"
#include "RegisterViewDelegate.h"
#include <QHeaderView>
#include <QTreeView>
#include <QMouseEvent>

//------------------------------------------------------------------------------
// Name: RegisterListWidget
// Desc:
//------------------------------------------------------------------------------
RegisterListWidget::RegisterListWidget(QWidget *parent) : QTreeView(parent) {

	// set the delegate
	setItemDelegate(new RegisterViewDelegate(this, this));

	// hide the exapander, since we provide our own
	setRootIsDecorated(false);

	header()->hide();

#if QT_VERSION<QT_VERSION_CHECK(5,0,0)
#define setSectionResizeMode setResizeMode
#endif
	header()->setSectionResizeMode(QHeaderView::ResizeToContents);
}

void RegisterListWidget::mousePressEvent(QMouseEvent* event)
{
	const auto index=indexAt(event->pos());
	if(index.isValid() && !model()->parent(index).isValid())
		setExpanded(index,!isExpanded(index)); // toggle expanded state of category item
	else
		QTreeView::mousePressEvent(event);
}


//------------------------------------------------------------------------------
// Name: isCategory
// Desc:
//------------------------------------------------------------------------------
void RegisterListWidget::setModel(QAbstractItemModel* model) {
	QTreeView::setModel(model);
	reset();
}

void RegisterListWidget::reset()
{
	QTreeView::reset();
	for(int row=0;row<model()->rowCount();++row)
		setFirstColumnSpanned(row,QModelIndex(),true);
}
