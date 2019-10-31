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

#ifndef ODBG_REG_VIEW_UTIL_H_20170817
#define ODBG_REG_VIEW_UTIL_H_20170817

#include "RegisterView.h"
#include <QAction>
#include <QSignalMapper>

namespace ODbgRegisterView {

static constexpr int MODEL_NAME_COLUMN    = RegisterViewModelBase::Model::NAME_COLUMN;
static constexpr int MODEL_VALUE_COLUMN   = RegisterViewModelBase::Model::VALUE_COLUMN;
static constexpr int MODEL_COMMENT_COLUMN = RegisterViewModelBase::Model::COMMENT_COLUMN;

template <class T>
T VALID_VARIANT(T variant) {
	static_assert(std::is_same<const typename std::remove_reference<T>::type, const QVariant>::value, "Wrong type passed to VALID_VARIANT");
	assert((variant).isValid());
	return variant;
}

template <class T>
T VALID_INDEX(T index) {
	static_assert(
		std::is_same<const typename std::remove_reference<T>::type, const QModelIndex>::value ||
		std::is_same<const typename std::remove_reference<T>::type, const QPersistentModelIndex>::value,
		"Wrong type passed to VALID_INDEX"
	);

	assert(index.isValid());
	return index;

}

template <class T, class P>
T *checked_cast(P p) {
	assert(dynamic_cast<T *>(p));
	return static_cast<T *>(p);
}

template <typename T>
T sqr(T v) {
	return v * v;
}

inline QPoint fieldPos(const FieldWidget *field) {
	// NOTE: mapToGlobal() is VERY slow, don't use it. Here we map to canvas, it's enough for all fields.
	return field->mapTo(field->parentWidget()->parentWidget(), QPoint());
}

// Square of Euclidean distance between two points
inline int distSqr(const QPoint &w1, const QPoint &w2) {
	return sqr(w1.x() - w2.x()) + sqr(w1.y() - w2.y());
}

inline QSize letterSize(const QFont &font) {
	const QFontMetrics fontMetrics(font);
	const int          width  = fontMetrics.width('w');
	const int          height = fontMetrics.height();
	return QSize(width, height);
}

inline QAction *newActionSeparator(QObject *parent) {
	const auto sep = new QAction(parent);
	sep->setSeparator(true);
	return sep;
}

inline QAction *newAction(const QString &text, QObject *parent, QObject *signalReceiver, const char *slot) {
	const auto action = new QAction(text, parent);
	QObject::connect(action, SIGNAL(triggered()), signalReceiver, slot);
	return action;
}

inline QAction *newAction(const QString &text, QObject *parent, QSignalMapper *mapper, int mapping) {
	const auto action = newAction(text, parent, mapper, SLOT(map()));
	mapper->setMapping(action, mapping);
	return action;
}

// TODO: switch from string-based search to enum-based one (add a new Role to model data)
inline QModelIndex findModelCategory(const  RegisterViewModelBase::Model *model, const QString &catToFind) {
	for (int row = 0; row < model->rowCount(); ++row) {
		const auto cat = model->index(row, 0).data(MODEL_NAME_COLUMN);
		if (cat.isValid() && cat.toString() == catToFind)
			return model->index(row, 0);
	}
	return QModelIndex();
}

// TODO: switch from string-based search to enum-based one (add a new Role to model data)
inline QModelIndex findModelRegister(QModelIndex categoryIndex,
                              const QString &regToFind,
							  int column = MODEL_NAME_COLUMN) {
	const auto model = categoryIndex.model();
	for (int row = 0; row < model->rowCount(categoryIndex); ++row) {
		const auto regIndex = model->index(row, MODEL_NAME_COLUMN, categoryIndex);
		const auto name     = model->data(regIndex).toString();
		if (name.toUpper() == regToFind) {
			if (column == MODEL_NAME_COLUMN)
				return regIndex;
			return regIndex.sibling(regIndex.row(), column);
		}
	}
	return QModelIndex();
}

inline QModelIndex getCommentIndex(const QModelIndex &nameIndex) {
	assert(nameIndex.isValid());
	return nameIndex.sibling(nameIndex.row(), MODEL_COMMENT_COLUMN);
}

inline QModelIndex getValueIndex(const QModelIndex &nameIndex) {
	assert(nameIndex.isValid());
	return nameIndex.sibling(nameIndex.row(), MODEL_VALUE_COLUMN);
}

}

#endif
