/*
Copyright (C) 2015 Ruslan Kabatsayev <b7.10110111@gmail.com>

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

#ifndef FIELD_WIDGET_H_20170818_
#define FIELD_WIDGET_H_20170818_

#include <QLabel>
#include <QModelIndex>

namespace ODbgRegisterView {

class ODBRegView;
class RegisterGroup;

class FieldWidget : public QLabel {
	Q_OBJECT

	void init(int fieldWidth);

protected:
	QPersistentModelIndex index_;
	int fieldWidth_;

	ODBRegView *regView() const;
	RegisterGroup *group() const;

public:
	FieldWidget(int fieldWidth, const QModelIndex &index_, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	FieldWidget(int fieldWidth, const QString &fixedText, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	explicit FieldWidget(const QString &fixedText, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());

public:
	virtual QString text() const;
	int lineNumber() const;
	int columnNumber() const;
	int fieldWidth() const;

public Q_SLOTS:
	virtual void adjustToData();
};

}

#endif
