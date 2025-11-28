/*
 * Copyright (C) 2015 Ruslan Kabatsayev <b7.10110111@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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

	[[nodiscard]] ODBRegView *regView() const;
	[[nodiscard]] RegisterGroup *group() const;

public:
	FieldWidget(int fieldWidth, const QModelIndex &index_, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	FieldWidget(int fieldWidth, const QString &fixedText, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	explicit FieldWidget(const QString &fixedText, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());

public:
	[[nodiscard]] virtual QString text() const;
	[[nodiscard]] int lineNumber() const;
	[[nodiscard]] int columnNumber() const;
	[[nodiscard]] int fieldWidth() const;

public Q_SLOTS:
	virtual void adjustToData();
};

}

#endif
