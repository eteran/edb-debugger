/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ResultsModel.h"
#include "edb.h"
#include <algorithm>

namespace ProcessPropertiesPlugin {

/**
 * @brief Constructs a ResultsModel with the specified parent object.
 *
 * @param parent The parent object for the model.
 */
ResultsModel::ResultsModel(QObject *parent)
	: QAbstractItemModel(parent) {
}

/**
 * @brief Gets the header data for the specified section, orientation, and role.
 *
 * @param section The section of the header to retrieve data for.
 * @param orientation The orientation of the header (horizontal or vertical).
 * @param role The role for which to retrieve data.
 * @return The header data for the specified section, orientation, and role.
 */
QVariant ResultsModel::headerData(int section, Qt::Orientation orientation, int role) const {

	if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
		switch (section) {
		case 0:
			return tr("Address");
		case 1:
			return tr("Type");
		case 2:
			return tr("String");
		}
	}

	return QVariant();
}

/**
 * @brief Gets the data for the specified index and role.
 *
 * @param index The index of the item to retrieve data for.
 * @param role The role for which to retrieve data.
 * @return The data for the specified index and role.
 */
QVariant ResultsModel::data(const QModelIndex &index, int role) const {

	if (!index.isValid()) {
		return QVariant();
	}

	const Result &result = results_[index.row()];

	if (role == Qt::DisplayRole) {
		switch (index.column()) {
		case 0:
			return edb::v1::format_pointer(result.address);
		case 1:
			switch (result.type) {
			case Result::Ascii:
				return tr("ASCII");
			case Result::Utf8:
				return tr("UTF8");
			case Result::Utf16:
				return tr("UTF16");
			case Result::Utf32:
				return tr("UTF32");
			}
			break;
		case 2:
			return result.string;
		default:
			return QVariant();
		}
	}

	return QVariant();
}

/**
 * @brief Adds a result to the ResultsModel instance.
 *
 * @param r The result to be added to the model.
 */
void ResultsModel::addResult(const Result &r) {
	beginInsertRows(QModelIndex(), rowCount(), rowCount());
	results_.push_back(r);
	endInsertRows();
}

/**
 * @brief Gets the index of the item at the specified row and column.
 *
 * @param row The row of the item to retrieve the index for.
 * @param column The column of the item to retrieve the index for.
 * @param parent The parent index.
 * @return The index of the item at the specified row and column.
 */
QModelIndex ResultsModel::index(int row, int column, const QModelIndex &parent) const {

	Q_UNUSED(parent)

	if (row >= results_.size()) {
		return QModelIndex();
	}

	if (column >= 3) {
		return QModelIndex();
	}

	if (row >= 0) {
		return createIndex(row, column, const_cast<Result *>(&results_[row]));
	}

	return createIndex(row, column);
}

/**
 * @brief Gets the parent index of the specified index.
 *
 * @param index The index for which to retrieve the parent.
 * @return The parent index.
 */
QModelIndex ResultsModel::parent(const QModelIndex &index) const {
	Q_UNUSED(index)
	return QModelIndex();
}

/**
 * @brief Gets the number of rows in the model.
 *
 * @param parent The parent index.
 * @return The number of rows in the model.
 */
int ResultsModel::rowCount(const QModelIndex &parent) const {
	Q_UNUSED(parent)
	return results_.size();
}

/**
 * @brief Gets the number of columns in the model.
 *
 * @param parent The parent index.
 * @return The number of columns in the model.
 */
int ResultsModel::columnCount(const QModelIndex &parent) const {
	Q_UNUSED(parent)
	return 3;
}

/**
 * @brief Sorts the model by the specified column and order.
 *
 * @param column The column to sort by.
 * @param order The order in which to sort (ascending or descending).
 */
void ResultsModel::sort(int column, Qt::SortOrder order) {

	if (order == Qt::AscendingOrder) {
		switch (column) {
		case 0:
			std::sort(results_.begin(), results_.end(), [](const Result &s1, const Result &s2) { return s1.address < s2.address; });
			break;
		case 1:
			std::sort(results_.begin(), results_.end(), [](const Result &s1, const Result &s2) { return s1.type < s2.type; });
			break;
		case 2:
			std::sort(results_.begin(), results_.end(), [](const Result &s1, const Result &s2) { return s1.string < s2.string; });
			break;
		}
	} else {
		switch (column) {
		case 0:
			std::sort(results_.begin(), results_.end(), [](const Result &s1, const Result &s2) { return s1.address > s2.address; });
			break;
		case 1:
			std::sort(results_.begin(), results_.end(), [](const Result &s1, const Result &s2) { return s1.type > s2.type; });
			break;
		case 2:
			std::sort(results_.begin(), results_.end(), [](const Result &s1, const Result &s2) { return s1.string < s2.string; });
			break;
		}
	}

	Q_EMIT dataChanged(createIndex(0, 0, nullptr), createIndex(-1, -1, nullptr));
}

}
