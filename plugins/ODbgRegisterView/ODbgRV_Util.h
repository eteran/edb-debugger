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

#ifndef ODBG_REG_VIEW_UTIL_H_20170817_
#define ODBG_REG_VIEW_UTIL_H_20170817_

#include "FieldWidget.h"
#include "RegisterGroup.h"
#include "RegisterViewModelBase.h"
#include "util/Font.h"
#include <QAction>

namespace ODbgRegisterView {

static constexpr int ModelNameColumn    = RegisterViewModelBase::Model::NAME_COLUMN;
static constexpr int ModelValueColumn   = RegisterViewModelBase::Model::VALUE_COLUMN;
static constexpr int ModelCommentColumn = RegisterViewModelBase::Model::COMMENT_COLUMN;

template <class T>
T valid_index(T index) {
	static_assert(std::is_same<const typename std::remove_reference<T>::type, const QModelIndex>::value ||
					  std::is_same<const typename std::remove_reference<T>::type, const QPersistentModelIndex>::value,
				  "Wrong type passed to valid_index");

	Q_ASSERT(index.isValid());
	return index;
}

template <class T, class P>
T *checked_cast(P p) {
	Q_ASSERT(dynamic_cast<T *>(p));
	return static_cast<T *>(p);
}

template <typename T>
constexpr T square(T v) {
	return v * v;
}

inline QPoint field_position(const FieldWidget *field) {
	// NOTE: mapToGlobal() is VERY slow, don't use it. Here we map to canvas, it's enough for all fields.
	return field->mapTo(field->parentWidget()->parentWidget(), QPoint());
}

// Square of Euclidean distance between two points
inline int distance_squared(const QPoint &w1, const QPoint &w2) {
	return square(w1.x() - w2.x()) + square(w1.y() - w2.y());
}

inline QSize letter_size(const QFont &font) {
	const QFontMetrics fontMetrics(font);
	const int width  = Font::maxWidth(fontMetrics);
	const int height = fontMetrics.height();
	return QSize(width, height);
}

inline QAction *new_action_separator(QObject *parent) {
	const auto sep = new QAction(parent);
	sep->setSeparator(true);
	return sep;
}

template <class Func>
inline QAction *new_action(const QString &text, QObject *parent, Func func) {
	const auto action = new QAction(text, parent);
	QObject::connect(action, &QAction::triggered, parent, func);
	return action;
}

// TODO: switch from string-based search to enum-based one (add a new Role to model data)
inline QModelIndex find_model_category(const RegisterViewModelBase::Model *model, const QString &catToFind) {
	for (int row = 0; row < model->rowCount(); ++row) {
		const QVariant cat = model->index(row, 0).data(ModelNameColumn);
		if (cat.isValid() && cat.toString() == catToFind) {
			return model->index(row, 0);
		}
	}

	return {};
}

// TODO: switch from string-based search to enum-based one (add a new Role to model data)
inline QModelIndex find_model_register(QModelIndex categoryIndex, const QString &regToFind, int column = ModelNameColumn) {

	const auto model = categoryIndex.model();
	for (int row = 0; row < model->rowCount(categoryIndex); ++row) {
		const auto regIndex = model->index(row, ModelNameColumn, categoryIndex);
		const auto name     = model->data(regIndex).toString();
		if (name.toUpper() == regToFind) {
			if (column == ModelNameColumn)
				return regIndex;
			return regIndex.sibling(regIndex.row(), column);
		}
	}
	return QModelIndex();
}

inline QModelIndex comment_index(const QModelIndex &nameIndex) {
	Q_ASSERT(nameIndex.isValid());
	return nameIndex.sibling(nameIndex.row(), ModelCommentColumn);
}

inline QModelIndex value_index(const QModelIndex &nameIndex) {
	Q_ASSERT(nameIndex.isValid());
	return nameIndex.sibling(nameIndex.row(), ModelValueColumn);
}

inline const QVariant &valid_variant(const QVariant &variant) {
	Q_ASSERT(variant.isValid());
	return variant;
}

}

#endif
