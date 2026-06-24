/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ResultsModel.h"
#include "edb.h"
#include <algorithm>

namespace ROPToolPlugin {

/**
 * @brief Constructs a ResultsModel with the specified parent object.
 *
 * @param parent The parent object for this model.
 */
ResultsModel::ResultsModel(QObject *parent)
	: QAbstractItemModel(parent) {
}

/**
 * @brief Returns the header data for the specified section, orientation, and role.
 *
 * @param section The section for which to retrieve header data.
 * @param orientation The orientation of the header.
 * @param role The role for which to retrieve data.
 * @return The header data for the specified section and role.
 */
QVariant ResultsModel::headerData(int section, Qt::Orientation orientation, int role) const {

	if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
		switch (section) {
		case 0:
			return tr("Address");
		case 1:
			return tr("Instruction");
		}
	}

	return QVariant();
}

/**
 * @brief Returns the data for the specified index and role.
 *
 * @param index The index for which to retrieve data.
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
			return result.instruction;
		default:
			return QVariant();
		}
	}

	return QVariant();
}

/**
 * @brief Adds a result to the model.
 *
 * @param r The result to add.
 */
void ResultsModel::addResult(const Result &r) {
	beginInsertRows(QModelIndex(), rowCount(), rowCount());
	results_.push_back(r);
	endInsertRows();
}

/**
 * @brief Returns the index of the item in the model specified by the given row, column and parent index.
 *
 * @param row The row of the item.
 * @param column The column of the item.
 * @param parent The parent index.
 * @return The index of the item.
 */
QModelIndex ResultsModel::index(int row, int column, const QModelIndex &parent) const {

	Q_UNUSED(parent)

	if (row >= results_.size()) {
		return QModelIndex();
	}

	if (column >= 2) {
		return QModelIndex();
	}

	if (row >= 0) {
		return createIndex(row, column, const_cast<Result *>(&results_[row]));
	}

	return createIndex(row, column);
}

/**
 * @brief Returns the parent index of the specified index.
 *
 * @param index The index for which to retrieve the parent.
 * @return The parent index.
 */
QModelIndex ResultsModel::parent(const QModelIndex &index) const {
	Q_UNUSED(index)
	return QModelIndex();
}

/**
 * @brief Returns the number of rows in the model.
 *
 * @param parent The parent index.
 * @return The number of rows in the model.
 */
int ResultsModel::rowCount(const QModelIndex &parent) const {
	Q_UNUSED(parent)
	return results_.size();
}

/**
 * @brief Returns the number of columns in the model.
 *
 * @param parent The parent index.
 * @return The number of columns in the model.
 */
int ResultsModel::columnCount(const QModelIndex &parent) const {
	Q_UNUSED(parent)
	return 2;
}

/**
 * @brief Sorts the model by the specified column and order.
 *
 * @param column The column by which to sort.
 * @param order The sort order.
 */
void ResultsModel::sort(int column, Qt::SortOrder order) {

	if (order == Qt::AscendingOrder) {
		switch (column) {
		case 0:
			std::sort(results_.begin(), results_.end(), [](const Result &s1, const Result &s2) { return s1.address < s2.address; });
			break;
		case 1:
			std::sort(results_.begin(), results_.end(), [](const Result &s1, const Result &s2) { return s1.instruction < s2.instruction; });
			break;
		}
	} else {
		switch (column) {
		case 0:
			std::sort(results_.begin(), results_.end(), [](const Result &s1, const Result &s2) { return s1.address > s2.address; });
			break;
		case 1:
			std::sort(results_.begin(), results_.end(), [](const Result &s1, const Result &s2) { return s1.instruction > s2.instruction; });
			break;
		}
	}

	Q_EMIT dataChanged(createIndex(0, 0, nullptr), createIndex(-1, -1, nullptr));
}

}
