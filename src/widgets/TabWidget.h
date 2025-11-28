/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TAB_WIDGET_H_20080118_
#define TAB_WIDGET_H_20080118_

#include <QTabWidget>

class TabWidget : public QTabWidget {
	Q_OBJECT

public:
	explicit TabWidget(QWidget *parent = nullptr);
	~TabWidget() override = default;

Q_SIGNALS:
	void customContextMenuRequested(int, const QPoint &);

public:
	void setData(int index, const QVariant &data);
	[[nodiscard]] QVariant data(int index) const;

protected:
	void mousePressEvent(QMouseEvent *event) override;
};

#endif
