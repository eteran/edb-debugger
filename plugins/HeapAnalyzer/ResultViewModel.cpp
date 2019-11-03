/*
Copyright (C) 2006 - 2015 Evan Teran
                          evan.teran@gmail.com

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

#include "ResultViewModel.h"
#include "edb.h"
#include <algorithm>

namespace HeapAnalyzerPlugin {

/**
 * @brief ResultViewModel::ResultViewModel
 * @param parent
 */
ResultViewModel::ResultViewModel(QObject *parent)
	: QAbstractItemModel(parent) {
}

/**
 * @brief ResultViewModel::headerData
 * @param section
 * @param orientation
 * @param role
 * @return
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
 * @brief ResultViewModel::data
 * @param index
 * @param role
 * @return
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
					return QString("dword ptr [%1]").arg(edb::v1::format_pointer(pointer));
				});
			} else {
				std::transform(result.pointers.begin(), result.pointers.end(), std::back_inserter(pointers), [](const edb::address_t pointer) -> QString {
					return QString("qword ptr [%1]").arg(edb::v1::format_pointer(pointer));
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
 * @brief ResultViewModel::addResult
 * @param r
 */
void ResultViewModel::addResult(const Result &r) {
	beginInsertRows(QModelIndex(), rowCount(), rowCount());
	results_.push_back(r);
	endInsertRows();
}

/**
 * @brief ResultViewModel::clearResults
 */
void ResultViewModel::clearResults() {
	beginResetModel();
	results_.clear();
	endResetModel();
}

/**
 * @brief ResultViewModel::index
 * @param row
 * @param column
 * @param parent
 * @return
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
	} else {
		return createIndex(row, column);
	}
}

/**
 * @brief ResultViewModel::parent
 * @param index
 * @return
 */
QModelIndex ResultViewModel::parent(const QModelIndex &index) const {
	Q_UNUSED(index)
	return QModelIndex();
}

/**
 * @brief ResultViewModel::rowCount
 * @param parent
 * @return
 */
int ResultViewModel::rowCount(const QModelIndex &parent) const {
	Q_UNUSED(parent)
	return results_.size();
}

/**
 * @brief ResultViewModel::columnCount
 * @param parent
 * @return
 */
int ResultViewModel::columnCount(const QModelIndex &parent) const {
	Q_UNUSED(parent)
	return 4;
}

/**
 * @brief ResultViewModel::setPointerData
 * @param index
 * @param data
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
