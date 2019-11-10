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

#include "TabWidget.h"

#include <QMouseEvent>
#include <QTabBar>

/**
 * @brief TabWidget::TabWidget
 * @param parent
 */
TabWidget::TabWidget(QWidget *parent)
	: QTabWidget(parent) {
}

/**
 * @brief TabWidget::setData
 * @param index
 * @param data
 */
void TabWidget::setData(int index, const QVariant &data) {
	tabBar()->setTabData(index, data);
}

/**
 * @brief TabWidget::data
 * @param index
 * @return
 */
QVariant TabWidget::data(int index) const {
	return tabBar()->tabData(index);
}

/**
 * @brief TabWidget::mousePressEvent
 * @param event
 */
void TabWidget::mousePressEvent(QMouseEvent *event) {
	if (event->button() != Qt::RightButton) {
		return;
	}

	const int tab = tabBar()->tabAt(event->pos());
	if (tab != -1) {
		Q_EMIT customContextMenuRequested(tab, event->pos());
	}
}
