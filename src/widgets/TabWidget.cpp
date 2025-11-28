/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
