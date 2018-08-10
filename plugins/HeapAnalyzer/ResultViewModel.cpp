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

//------------------------------------------------------------------------------
// Name: ResultViewModel
// Desc:
//------------------------------------------------------------------------------
ResultViewModel::ResultViewModel(QObject *parent) : QAbstractItemModel(parent), updates_enabled_(false) {
}

//------------------------------------------------------------------------------
// Name: headerData
// Desc:
//------------------------------------------------------------------------------
QVariant ResultViewModel::headerData(int section, Qt::Orientation orientation, int role) const {

	if(role == Qt::DisplayRole && orientation == Qt::Horizontal) {
		switch(section) {
		case 0: return tr("Block");
		case 1: return tr("Size");
		case 2: return tr("Type");
		case 3: return tr("Data");
		}
	}

	return QVariant();
}

//------------------------------------------------------------------------------
// Name: data
// Desc:
//------------------------------------------------------------------------------
QVariant ResultViewModel::data(const QModelIndex &index, int role) const {

	if(!index.isValid())
		return QVariant();

	if(role != Qt::DisplayRole)
		return QVariant();

	const Result &result = results_[index.row()];

	switch(index.column()) {
	case 0:  return edb::v1::format_pointer(result.block);
	case 1:  return edb::v1::format_pointer(result.size);
	case 2:  return result.type;
	case 3:  return result.data;
	default: return QVariant();
	}
}

//------------------------------------------------------------------------------
// Name: addResult
// Desc:
//------------------------------------------------------------------------------
void ResultViewModel::addResult(const Result &r) {
	results_.push_back(r);
	update();
}

//------------------------------------------------------------------------------
// Name: clearResults
// Desc:
//------------------------------------------------------------------------------
void ResultViewModel::clearResults() {
	results_.clear();
	update();
}

//------------------------------------------------------------------------------
// Name: update
// Desc:
//------------------------------------------------------------------------------
void ResultViewModel::update() {
	if(updates_enabled_) {
		beginResetModel();
		endResetModel();
	}
}

//------------------------------------------------------------------------------
// Name: index
// Desc:
//------------------------------------------------------------------------------
QModelIndex ResultViewModel::index(int row, int column, const QModelIndex &parent) const {

	Q_UNUSED(parent);

	if(row >= results_.size()) {
		return QModelIndex();
	}

	if(column >= 4) {
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
QModelIndex ResultViewModel::parent(const QModelIndex &index) const {
	Q_UNUSED(index);
	return QModelIndex();
}

//------------------------------------------------------------------------------
// Name: rowCount
// Desc:
//------------------------------------------------------------------------------
int ResultViewModel::rowCount(const QModelIndex &parent) const {
	Q_UNUSED(parent);
	return results_.size();
}

//------------------------------------------------------------------------------
// Name: columnCount
// Desc:
//------------------------------------------------------------------------------
int ResultViewModel::columnCount(const QModelIndex &parent) const {
	Q_UNUSED(parent);
	return 4;
}

//------------------------------------------------------------------------------
// Name: sort
// Desc:
//------------------------------------------------------------------------------
void ResultViewModel::sort(int column, Qt::SortOrder order) {

	if(order == Qt::AscendingOrder) {
		switch(column) {
		case 0: std::sort(results_.begin(), results_.end(), [](const Result &s1, const Result &s2) { return s1.block < s2.block; }); break;
		case 1: std::sort(results_.begin(), results_.end(), [](const Result &s1, const Result &s2) { return s1.size < s2.size; });   break;
		case 2: std::sort(results_.begin(), results_.end(), [](const Result &s1, const Result &s2) { return s1.type < s2.type; });   break;
		case 3: std::sort(results_.begin(), results_.end(), [](const Result &s1, const Result &s2)  { return s1.data < s2.data; });  break;
		}
	} else {
		switch(column) {
		case 0: std::sort(results_.begin(), results_.end(), [](const Result &s1, const Result &s2) { return s1.block > s2.block; }); break;
		case 1: std::sort(results_.begin(), results_.end(), [](const Result &s1, const Result &s2) { return s1.size > s2.size; });   break;
		case 2: std::sort(results_.begin(), results_.end(), [](const Result &s1, const Result &s2) { return s1.type > s2.type; });   break;
		case 3: std::sort(results_.begin(), results_.end(), [](const Result &s1, const Result &s2) { return s1.data > s2.data; });   break;
		}
	}

	Q_EMIT dataChanged(createIndex(0, 0, nullptr), createIndex(-1, -1, nullptr));
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
void ResultViewModel::setUpdatesEnabled(bool value) {
	updates_enabled_ = value;
	if(updates_enabled_) {
		update();
	}
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
bool ResultViewModel::updatesEnabled() const {
	return updates_enabled_;
}

}
