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

#ifndef VALUE_FIELD_H_20170818_
#define VALUE_FIELD_H_20170818_

#include "FieldWidget.h"
#include "RegisterViewModelBase.h"

#include <functional>

class QAction;
class QMouseEvent;

namespace ODbgRegisterView {

class ValueField : public FieldWidget {
	Q_OBJECT

private:
	bool selected_ = false;
	bool hovered_  = false;
	std::function<QString(QString)> valueFormatter_;

	// For GPR
	QAction *setToZeroAction_ = nullptr;
	QAction *setToOneAction_  = nullptr;

protected:
	QList<QAction *> menuItems_;

private:
	void init();
	QColor fgColorForChangedField() const;
	void editNormalReg(const QModelIndex &indexToEdit, const QModelIndex &clickedIndex) const;

protected:
	RegisterViewModelBase::Model *model() const;
	[[nodiscard]] bool changed() const;

	void enterEvent(QEvent *) override;
	void leaveEvent(QEvent *) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseDoubleClickEvent(QMouseEvent *event) override;
	void paintEvent(QPaintEvent *event) override;

	[[nodiscard]] ValueField *bestNeighbor(const std::function<bool(const QPoint &neighborPos, const ValueField *curResult, const QPoint &selfPos)> &firstIsBetter) const;

public:
	ValueField(int fieldWidth, const QModelIndex &index_, const std::function<QString(const QString &)> &valueFormatter_, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	ValueField(int fieldWidth, const QModelIndex &index_, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	[[nodiscard]] ValueField *up() const;
	[[nodiscard]] ValueField *down() const;
	[[nodiscard]] ValueField *left() const;
	[[nodiscard]] ValueField *right() const;

	[[nodiscard]] bool isSelected() const;
	void showMenu(const QPoint &position);
	QString text() const override;
	[[nodiscard]] QModelIndex regIndex() const;

public Q_SLOTS:
	void defaultAction();
#if defined(EDB_X86) || defined(EDB_X86_64)
	void pushFPUStack();
	void popFPUStack();
#endif
	void adjustToData() override;
	void select();
	void unselect();
	virtual void updatePalette();
	void copyToClipboard() const;
	void setZero();
	void setToOne();
	void increment();
	void decrement();
	void invert();

Q_SIGNALS:
	void selected();
};

}

#endif
