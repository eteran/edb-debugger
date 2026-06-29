/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ResultViewModel.h"
#include "edb.h"
#include <algorithm>

namespace HeapAnalyzerPlugin {

/**
 * @brief Constructs a ResultViewModel instance with the specified parent object.
 *
 * @param parent The parent object for the model.
 */
ResultViewModel::ResultViewModel(QObject *parent)
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
QVariant ResultViewModel::headerData(int section, Qt::Orientation orientation, int role) const {

	if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
		switch (section) {
		case 0:
			return tr("Block");
		case 1:
			return tr("Size");
		case 2:
			return tr("Type");
		case 3:
			return tr("Data");
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
QVariant ResultViewModel::data(const QModelIndex &index, int role) const {

	if (!index.isValid()) {
		return QVariant();
	}

	if (role != Qt::DisplayRole) {
		return QVariant();
	}

	const Result &result = results_[index.row()];

	switch (index.column()) {
	case 0:
		return edb::v1::format_pointer(result.address);
	case 1:
		return edb::v1::format_pointer(result.size);
	case 2:
		switch (result.type) {
		case Result::Top:
			return tr("Top");
		case Result::Busy:
			return tr("Busy");
		case Result::Free:
			return tr("Free");
		}
		return QVariant();
	case 3: {
		switch (result.dataType) {
		case Result::Pointer: {
			QStringList pointers;
			if (edb::v1::debuggeeIs32Bit()) {
				std::transform(result.pointers.begin(), result.pointers.end(), std::back_inserter(pointers), [](const edb::address_t pointer) -> QString {
					return QStringLiteral("dword ptr [%1]").arg(edb::v1::format_pointer(pointer));
				});
			} else {
				std::transform(result.pointers.begin(), result.pointers.end(), std::back_inserter(pointers), [](const edb::address_t pointer) -> QString {
					return QStringLiteral("qword ptr [%1]").arg(edb::v1::format_pointer(pointer));
				});
			}
			return pointers.join("|");
		}
		case Result::Png:
			return tr("PNG IMAGE");
		case Result::Xpm:
			return tr("XPM IMAGE");
		case Result::Bzip:
			return tr("BZIP FILE");
		case Result::Compress:
			return tr("COMPRESS FILE");
		case Result::Gzip:
			return tr("GZIP FILE");
		case Result::Ascii:
			return tr("ASCII \"%1\"").arg(result.data);
		case Result::Utf16:
			return tr("UTF-16 \"%1\"").arg(result.data);
		case Result::Unknown:
			return QVariant();
		}
		return QVariant();
	}
	default:
		return QVariant();
	}
}

/**
 * @brief Adds a result to the ResultViewModel instance. This function is used to populate the model with new results.
 *
 * @param r The result to be added to the model.
 */
void ResultViewModel::addResult(const Result &r) {
	beginInsertRows(QModelIndex(), rowCount(), rowCount());
	results_.push_back(r);
	endInsertRows();
}

/**
 * @brief Clears all results from the ResultViewModel instance.
 */
void ResultViewModel::clearResults() {
	beginResetModel();
	results_.clear();
	endResetModel();
}

/**
 * @brief Gets the index of the item at the specified row and column.
 *
 * @param row The row of the item to retrieve the index for.
 * @param column The column of the item to retrieve the index for.
 * @param parent The parent index.
 * @return The index of the item at the specified row and column.
 */
QModelIndex ResultViewModel::index(int row, int column, const QModelIndex &parent) const {

	Q_UNUSED(parent)

	if (row >= results_.size()) {
		return QModelIndex();
	}

	if (column >= 4) {
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
QModelIndex ResultViewModel::parent(const QModelIndex &index) const {
	Q_UNUSED(index)
	return QModelIndex();
}

/**
 * @brief Gets the number of rows in the model.
 *
 * @param parent The parent index.
 * @return The number of rows in the model.
 */
int ResultViewModel::rowCount(const QModelIndex & /*parent*/) const {
	return static_cast<int>(results_.size());
}

/**
 * @brief Gets the number of columns in the model.
 *
 * @param parent The parent index.
 * @return The number of columns in the model.
 */
int ResultViewModel::columnCount(const QModelIndex & /*parent*/) const {
	return 4;
}

/**
 * @brief Sets the pointer data for the specified index.
 *
 * @param index The index for which to set pointer data.
 * @param pointers The vector of pointers to set.
 */
void ResultViewModel::setPointerData(const QModelIndex &index, const std::vector<edb::address_t> &pointers) {
	if (!index.isValid()) {
		return;
	}

	Result &result = results_[index.row()];

	result.pointers = pointers;
	result.dataType = Result::Pointer;
	Q_EMIT dataChanged(index, index);
}

}
