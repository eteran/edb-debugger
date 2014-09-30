/*
Copyright (C) 2014 - 2014 Evan Teran
                          eteran@alum.rit.edu

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

#include "ThreadsModel.h"
#include <QtAlgorithms>

ThreadsModel::ThreadsModel(QObject *parent) : QAbstractItemModel(parent) {
}

ThreadsModel::~ThreadsModel() {
}

QModelIndex ThreadsModel::index(int row, int column, const QModelIndex &parent) const {
	Q_UNUSED(parent);

	if(row >= rowCount(parent) || column >= columnCount(parent)) {
		return QModelIndex();
	}

	if(row >= 0) {
		return createIndex(row, column, const_cast<Item *>(&items_[row]));
	} else {
		return createIndex(row, column);
	}
}

QModelIndex ThreadsModel::parent(const QModelIndex &index) const {
	Q_UNUSED(index);
	return QModelIndex();
}

QVariant ThreadsModel::data(const QModelIndex &index, int role) const {

	if(index.isValid()) {

		const Item &item = items_[index.row()];

		if(role == Qt::DisplayRole) {
			switch(index.column()) {
			case 0:
				if(item.current) {
					return tr("*%1").arg(item.tid);
				} else {
					return item.tid;
				}
			}
		} else if(role == Qt::UserRole) {
			return item.tid;
		}
	}

	return QVariant();
}

QVariant ThreadsModel::headerData(int section, Qt::Orientation orientation, int role) const {

	if(role == Qt::DisplayRole && orientation == Qt::Horizontal) {
		switch(section) {
		case 0:
			return tr("Thread ID");
		}
	}

	return QVariant();
}

int ThreadsModel::columnCount(const QModelIndex &parent) const {
	Q_UNUSED(parent);
	return 1;
}

int ThreadsModel::rowCount(const QModelIndex &parent) const {
	Q_UNUSED(parent);
	return items_.size();
}

void ThreadsModel::addThread(edb::tid_t tid, bool current) {
	beginInsertRows(QModelIndex(), rowCount(), rowCount());

	const Item item = {
		tid, current
	};
	items_.push_back(item);
	endInsertRows();
}

void ThreadsModel::clear() {
	beginRemoveRows(QModelIndex(), 0, rowCount());
	items_.clear();
	endRemoveRows();
}
