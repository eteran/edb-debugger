/*
Copyright (C) 2006 - 2019 Evan Teran
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

#include "ResultsModel.h"
#include "edb.h"
#include <algorithm>

namespace FunctionFinderPlugin {

//------------------------------------------------------------------------------
// Name: BookmarksModel
// Desc:
//------------------------------------------------------------------------------
ResultsModel::ResultsModel(QObject *parent) : QAbstractItemModel(parent) {
}

//------------------------------------------------------------------------------
// Name: headerData
// Desc:
//------------------------------------------------------------------------------
QVariant ResultsModel::headerData(int section, Qt::Orientation orientation, int role) const {

	if(role == Qt::DisplayRole && orientation == Qt::Horizontal) {
		switch(section) {
		case 0: return tr("Start Address");
		case 1: return tr("End Address");
		case 2: return tr("Size");
		case 3: return tr("Score");
		case 4: return tr("Type");
		case 5: return tr("Symbol");
		}
	}

	return QVariant();
}

//------------------------------------------------------------------------------
// Name: data
// Desc:
//------------------------------------------------------------------------------
QVariant ResultsModel::data(const QModelIndex &index, int role) const {

	if(!index.isValid()) {
		return QVariant();
	}

	const Result &result = results_[index.row()];

	if(role == Qt::DisplayRole) {
		switch(index.column()) {
		case 0:  return edb::v1::format_pointer(result.start_address);
		case 1:  return edb::v1::format_pointer(result.end_address);
		case 2:  return static_cast<quint64>(result.size);
		case 3:  return result.score;
		case 4:  return result.type == Function::FUNCTION_THUNK ? tr("Thunk") : tr("Standard Function");
		case 5:  return result.symbol;
		default: return QVariant();
		}
	}

	return QVariant();
}

//------------------------------------------------------------------------------
// Name: addBookmark
// Desc:
//------------------------------------------------------------------------------
void ResultsModel::addResult(const Result &r) {
	beginInsertRows(QModelIndex(), rowCount(), rowCount());
	results_.push_back(r);
	endInsertRows();
}


//------------------------------------------------------------------------------
// Name: index
// Desc:
//------------------------------------------------------------------------------
QModelIndex ResultsModel::index(int row, int column, const QModelIndex &parent) const {

	Q_UNUSED(parent)

	if(row >= results_.size()) {
		return QModelIndex();
	}

	if(column >= 6) {
		return QModelIndex();
	}

	if(row >= 0) {
		return createIndex(row, column, const_cast<Result *>(&results_[row]));
	} else {
		return createIndex(row, column);
	}
}

//------------------------------------------------------------------------------
// Name: parent
// Desc:
//------------------------------------------------------------------------------
QModelIndex ResultsModel::parent(const QModelIndex &index) const {
	Q_UNUSED(index)
	return QModelIndex();
}

//------------------------------------------------------------------------------
// Name: rowCount
// Desc:
//------------------------------------------------------------------------------
int ResultsModel::rowCount(const QModelIndex &parent) const {
	Q_UNUSED(parent)
	return results_.size();
}

//------------------------------------------------------------------------------
// Name: columnCount
// Desc:
//------------------------------------------------------------------------------
int ResultsModel::columnCount(const QModelIndex &parent) const {
	Q_UNUSED(parent)
	return 6;
}

//------------------------------------------------------------------------------
// Name: sort
// Desc:
//------------------------------------------------------------------------------
void ResultsModel::sort(int column, Qt::SortOrder order) {

	if(order == Qt::AscendingOrder) {
		switch(column) {
		case 0: std::sort(results_.begin(), results_.end(), [](const Result &s1, const Result &s2) { return s1.start_address < s2.start_address; }); break;
		case 1: std::sort(results_.begin(), results_.end(), [](const Result &s1, const Result &s2) { return s1.end_address < s2.end_address; });   break;
		case 2: std::sort(results_.begin(), results_.end(), [](const Result &s1, const Result &s2) { return s1.size < s2.size; });   break;
		case 3: std::sort(results_.begin(), results_.end(), [](const Result &s1, const Result &s2) { return s1.score < s2.score; });  break;
		case 4: std::sort(results_.begin(), results_.end(), [](const Result &s1, const Result &s2) { return s1.type < s2.type; });  break;
		case 5: std::sort(results_.begin(), results_.end(), [](const Result &s1, const Result &s2) { return s1.symbol < s2.symbol; });  break;
		}
	} else {
		switch(column) {
		case 0: std::sort(results_.begin(), results_.end(), [](const Result &s1, const Result &s2) { return s1.start_address > s2.start_address; }); break;
		case 1: std::sort(results_.begin(), results_.end(), [](const Result &s1, const Result &s2) { return s1.end_address > s2.end_address; });   break;
		case 2: std::sort(results_.begin(), results_.end(), [](const Result &s1, const Result &s2) { return s1.size > s2.size; });   break;
		case 3: std::sort(results_.begin(), results_.end(), [](const Result &s1, const Result &s2) { return s1.score > s2.score; });   break;
		case 4: std::sort(results_.begin(), results_.end(), [](const Result &s1, const Result &s2) { return s1.type > s2.type; });   break;
		case 5: std::sort(results_.begin(), results_.end(), [](const Result &s1, const Result &s2) { return s1.symbol > s2.symbol; });   break;
		}
	}

	Q_EMIT dataChanged(createIndex(0, 0, nullptr), createIndex(-1, -1, nullptr));
}

}
