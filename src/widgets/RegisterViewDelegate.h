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

#ifndef REGISTER_VIEW_DELEGATE_H_20070519_
#define REGISTER_VIEW_DELEGATE_H_20070519_

#include <QStyledItemDelegate>

class QTreeView;

class RegisterViewDelegate : public QStyledItemDelegate {
	Q_OBJECT

public:
	RegisterViewDelegate(QTreeView *view, QWidget *parent);
	RegisterViewDelegate(const RegisterViewDelegate &) = delete;
	RegisterViewDelegate &operator=(const RegisterViewDelegate &) = delete;
	~RegisterViewDelegate() override                              = default;

public:
	void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
	QSize sizeHint(const QStyleOptionViewItem &opt, const QModelIndex &index) const override;

private:
	QTreeView *view_;
};

#endif
