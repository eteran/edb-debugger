/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef REGISTER_VIEW_DELEGATE_H_20070519_
#define REGISTER_VIEW_DELEGATE_H_20070519_

#include <QStyledItemDelegate>

class QTreeView;

class RegisterViewDelegate : public QStyledItemDelegate {
	Q_OBJECT

public:
	RegisterViewDelegate(QTreeView *view, QWidget *parent);
	RegisterViewDelegate(const RegisterViewDelegate &)            = delete;
	RegisterViewDelegate &operator=(const RegisterViewDelegate &) = delete;
	~RegisterViewDelegate() override                              = default;

public:
	void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
	[[nodiscard]] QSize sizeHint(const QStyleOptionViewItem &opt, const QModelIndex &index) const override;

private:
	QTreeView *view_;
};

#endif
