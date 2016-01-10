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
#if QT_VERSION >= 0x050000
	header()->setSectionResizeMode(QHeaderView::Stretch);
#else
	header()->setResizeMode(QHeaderView::Stretch);
#endif
	
}

//------------------------------------------------------------------------------
// Name: ~RegisterListWidget
// Desc:
//------------------------------------------------------------------------------
RegisterListWidget::~RegisterListWidget() {

}

//------------------------------------------------------------------------------
// Name: handleMousePress
// Desc:
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Name: mouseDoubleClickEvent
// Desc:
//------------------------------------------------------------------------------
void RegisterListWidget::mouseDoubleClickEvent(QMouseEvent *event) {
	const auto index=indexAt(event->pos());
	if(index.isValid() && !model()->parent(index).isValid())
		setExpanded(index,!isExpanded(index)); // toggle expanded state of category item
	QTreeView::mouseDoubleClickEvent(event);
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
	// FIXME: this is a simplistic approach at making all categories open by default.
	//        Maybe it's better to save which rows have to be expanded based on user actions and act accordingly.
	for(int row=0;row<model()->rowCount();++row)
	{
		setExpanded(model()->index(row,0),true);
		setFirstColumnSpanned(row,QModelIndex(),true);
	}
	header()->setResizeMode(QHeaderView::ResizeToContents);
	header()->setStretchLastSection(false);
}
