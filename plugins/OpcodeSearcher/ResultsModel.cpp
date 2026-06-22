/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ResultsModel.h"
#include "edb.h"
#include <algorithm>

namespace OpcodeSearcherPlugin {

/**
 * @brief Constructs a ResultsModel object with the given parent.
 *
 * @param parent The parent QObject of this model.
 */
ResultsModel::ResultsModel(QObject *parent)
	: QAbstractItemModel(parent) {
}

/**
 * @brief Returns the header data for the given section, orientation, and role.
 *
 * @param section The section of the header to return data for.
 * @param orientation The orientation of the header (horizontal or vertical).
 * @param role The role for which to return data.
 * @return the header data for the given section, orientation, and role, or an invalid QVariant if the role is not DisplayRole or the orientation is not Horizontal.
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
 * @brief Returns the data for the given index and role.
 *
 * @param index The index for which to return data.
 * @param role The role for which to return data.
 * @return the data for the given index and role, or an invalid QVariant if the index is not valid or the role is not DisplayRole.
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
 * @brief Adds a new result to the model.
 *
 * @param r The result to add to the model.
 */
void ResultsModel::addResult(const Result &r) {
	beginInsertRows(QModelIndex(), rowCount(), rowCount());
	results_.push_back(r);
	endInsertRows();
}

/**
 * @brief Returns the index for the given row, column, and parent.
 *
 * @param row The row of the index to return.
 * @param column The column of the index to return.
 * @param parent The parent index of the index to return.
 * @return the index for the given row, column, and parent, or an invalid QModelIndex if the row or column is out of bounds.
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
 * @brief Returns the parent index for the given index.
 *
 * @param index The index for which to return the parent.
 * @return The parent index for the given index, or an invalid QModelIndex if the index is not valid.
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
 * @brief Sorts the model by the given column and order.
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
