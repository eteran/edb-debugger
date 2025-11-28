/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef REGISTER_GROUP_H_20191119_
#define REGISTER_GROUP_H_20191119_

#include <QWidget>

namespace ODbgRegisterView {

class SimdValueManager;
class FieldWidget;
class ValueField;
class ODBRegView;

class RegisterGroup : public QWidget {
	Q_OBJECT
	friend SimdValueManager;

public:
	explicit RegisterGroup(const QString &name_, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());

public:
	[[nodiscard]] QList<FieldWidget *> fields() const;
	[[nodiscard]] QList<ValueField *> valueFields() const;
	[[nodiscard]] QMargins getFieldMargins() const;
	void appendNameValueComment(const QModelIndex &nameIndex, const QString &tooltip = "", bool insertComment = true);
	// Insert, but without moving to its place
	void insert(FieldWidget *widget);
	void insert(int line, int column, FieldWidget *widget);
	void setupPositionAndSize(int line, int column, FieldWidget *widget);
	void showMenu(const QPoint &position, const QList<QAction *> &additionalItems = {}) const;

public Q_SLOTS:
	void adjustWidth();

protected:
	void mousePressEvent(QMouseEvent *event) override;

private:
	[[nodiscard]] int lineAfterLastField() const;
	[[nodiscard]] ODBRegView *regView() const;

private:
	QList<QAction *> menuItems_;
	QString name_;
};

}

#endif
