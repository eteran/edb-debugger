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
 * @brief ResultsModel::ResultsModel
 * @param parent
 */
ResultsModel::ResultsModel(QObject *parent)
	: QAbstractItemModel(parent) {
}

/**
 * @brief ResultsModel::headerData
 * @param section
 * @param orientation
 * @param role
 * @return
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
 * @brief ResultsModel::data
 * @param index
 * @param role
 * @return
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
 * @brief ResultsModel::addResult
 * @param r
 */
void ResultsModel::addResult(const Result &r) {
	beginInsertRows(QModelIndex(), rowCount(), rowCount());
	results_.push_back(r);
	endInsertRows();
}

/**
 * @brief ResultsModel::index
 * @param row
 * @param column
 * @param parent
 * @return
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
 * @brief ResultsModel::parent
 * @param index
 * @return
 */
QModelIndex ResultsModel::parent(const QModelIndex &index) const {
	Q_UNUSED(index)
	return QModelIndex();
}

/**
 * @brief ResultsModel::rowCount
 * @param parent
 * @return
 */
int ResultsModel::rowCount(const QModelIndex &parent) const {
	Q_UNUSED(parent)
	return results_.size();
}

/**
 * @brief ResultsModel::columnCount
 * @param parent
 * @return
 */
int ResultsModel::columnCount(const QModelIndex &parent) const {
	Q_UNUSED(parent)
	return 2;
}

/**
 * @brief ResultsModel::sort
 * @param column
 * @param order
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
