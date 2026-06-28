/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ResultsModel.h"
#include "edb.h"
#include <algorithm>

namespace FunctionFinderPlugin {

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
			return tr("Start Address");
		case 1:
			return tr("End Address");
		case 2:
			return tr("Size");
		case 3:
			return tr("Score");
		case 4:
			return tr("Type");
		case 5:
			return tr("Symbol");
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
			return edb::v1::format_pointer(result.startAddress);
		case 1:
			return edb::v1::format_pointer(result.endAddress);
		case 2:
			return static_cast<quint64>(result.size);
		case 3:
			return result.score;
		case 4:
			return result.type == Function::Thunk ? tr("Thunk") : tr("Standard Function");
		case 5:
			return result.symbol;
		default:
			return QVariant();
		}
	}

	return QVariant();
}

/**
 * @brief Adds a result to the results model. This function is used to populate the results table with new search results.
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

	if (column >= 6) {
		return QModelIndex();
	}

	if (row >= 0) {
		return createIndex(row, column, const_cast<Result *>(&results_[row]));
	}

	return createIndex(row, column);
}

/**
 * @brief Gets the parent index of the specified index. Since this model is a flat list, it always returns an invalid index.
 *
 * @param index The index for which to retrieve the parent index.
 * @return An invalid QModelIndex, since this model is a flat list.
 */
QModelIndex ResultsModel::parent(const QModelIndex & /*index*/) const {
	return QModelIndex();
}

/**
 * @brief Gets the number of rows in the model.
 *
 * @param parent The parent index.
 * @return The number of rows in the model.
 */

int ResultsModel::rowCount(const QModelIndex & /*parent*/) const {
	return static_cast<int>(results_.size());
}

/**
 * @brief Gets the number of columns in the model. This model has a fixed number of columns (6).
 *
 * @param parent The parent index.
 * @return The number of columns in the model (6).
 */
int ResultsModel::columnCount(const QModelIndex & /*parent*/) const {
	return 6;
}

/**
 * @brief Sorts the results in the model based on the specified column and order. The sorting is performed in-place, and the model is updated accordingly.
 *
 * @param column The column to sort by.
 * @param order The sort order.
 */
void ResultsModel::sort(int column, Qt::SortOrder order) {

	if (order == Qt::AscendingOrder) {
		switch (column) {
		case 0:
			std::sort(results_.begin(), results_.end(), [](const Result &s1, const Result &s2) { return s1.startAddress < s2.startAddress; });
			break;
		case 1:
			std::sort(results_.begin(), results_.end(), [](const Result &s1, const Result &s2) { return s1.endAddress < s2.endAddress; });
			break;
		case 2:
			std::sort(results_.begin(), results_.end(), [](const Result &s1, const Result &s2) { return s1.size < s2.size; });
			break;
		case 3:
			std::sort(results_.begin(), results_.end(), [](const Result &s1, const Result &s2) { return s1.score < s2.score; });
			break;
		case 4:
			std::sort(results_.begin(), results_.end(), [](const Result &s1, const Result &s2) { return s1.type < s2.type; });
			break;
		case 5:
			std::sort(results_.begin(), results_.end(), [](const Result &s1, const Result &s2) { return s1.symbol < s2.symbol; });
			break;
		}
	} else {
		switch (column) {
		case 0:
			std::sort(results_.begin(), results_.end(), [](const Result &s1, const Result &s2) { return s1.startAddress > s2.startAddress; });
			break;
		case 1:
			std::sort(results_.begin(), results_.end(), [](const Result &s1, const Result &s2) { return s1.endAddress > s2.endAddress; });
			break;
		case 2:
			std::sort(results_.begin(), results_.end(), [](const Result &s1, const Result &s2) { return s1.size > s2.size; });
			break;
		case 3:
			std::sort(results_.begin(), results_.end(), [](const Result &s1, const Result &s2) { return s1.score > s2.score; });
			break;
		case 4:
			std::sort(results_.begin(), results_.end(), [](const Result &s1, const Result &s2) { return s1.type > s2.type; });
			break;
		case 5:
			std::sort(results_.begin(), results_.end(), [](const Result &s1, const Result &s2) { return s1.symbol > s2.symbol; });
			break;
		}
	}

	Q_EMIT dataChanged(createIndex(0, 0, nullptr), createIndex(-1, -1, nullptr));
}

}
