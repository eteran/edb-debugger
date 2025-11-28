/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef BOOKMARK_WIDGET_H_20101207_
#define BOOKMARK_WIDGET_H_20101207_

#include "BookmarksModel.h"
#include "Types.h"
#include "ui_BookmarkWidget.h"
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
	void on_tableView_doubleClicked(const QModelIndex &index);
	void on_tableView_customContextMenuRequested(const QPoint &pos);

public:
	void shortcut(int index);
	void addAddress(edb::address_t address, const QString &type = QString(), const QString &comment = QString());
	[[nodiscard]] QList<BookmarksModel::Bookmark> entries() const;

private:
	void buttonAddClicked();
	void buttonDelClicked();
	void buttonClearClicked();
	void toggleBreakpoint();
	void addConditionalBreakpoint();

private:
	template <class F>
	QAction *createAction(const QString &text, const QKeySequence &keySequence, F func);

private:
	Ui::BookmarkWidget ui;
	BookmarksModel *model_ = nullptr;
	QAction *toggleBreakpointAction_;
	QAction *conditionalBreakpointAction_;
};

}

#endif
