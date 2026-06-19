/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "TabWidget.h"

#include <QMouseEvent>
#include <QTabBar>

/**
 * @brief Constructs the tab widget and initializes its internal state.
 * @param parent The parent widget for the tab widget.
 */
TabWidget::TabWidget(QWidget *parent)
	: QTabWidget(parent) {
}

/**
 * @brief Sets the data for a specific tab.
 * @param index The index of the tab.
 * @param data The data to set for the tab.
 */
void TabWidget::setData(int index, const QVariant &data) {
	tabBar()->setTabData(index, data);
}

/**
 * @brief Returns the data for a specific tab.
 * @param index The index of the tab.
 * @return The data for the tab.
 */
QVariant TabWidget::data(int index) const {
	return tabBar()->tabData(index);
}

/**
 * @brief Handles mouse press events for the tab widget.
 * @param event The mouse event.
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
