/*
Copyright (C) 2014 - 2015 Evan Teran
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

#include "ThreadsModel.h"
#include "edb.h"
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
					return tr("*%1").arg(item.thread->tid());
				} else {
					return item.thread->tid();
				}
			case 1:
				return item.thread->priority();
			case 2:
				{
					const QString default_region_name;
					const QString symname = edb::v1::find_function_symbol(item.thread->instruction_pointer(), default_region_name);

					if(!symname.isEmpty()) {
						return QString("%1 <%2>").arg(edb::v1::format_pointer(item.thread->instruction_pointer())).arg(symname);
					} else {
						return QString("%1").arg(edb::v1::format_pointer(item.thread->instruction_pointer()));
					}				
				}
			case 3:
				return item.thread->runState();
			case 4:
				return item.thread->name();								
			}
		} else if(role == Qt::UserRole) {
			return item.thread->tid();
		}
	}

	return QVariant();
}

QVariant ThreadsModel::headerData(int section, Qt::Orientation orientation, int role) const {

	if(role == Qt::DisplayRole && orientation == Qt::Horizontal) {
		switch(section) {
		case 0:
			return tr("ID");
		case 1:
			return tr("Priority");
		case 2:
			return tr("Instruction Pointer");
		case 3:
			return tr("State");			
		case 4:
			return tr("Name");			
		}
	}

	return QVariant();
}

int ThreadsModel::columnCount(const QModelIndex &parent) const {
	Q_UNUSED(parent);
	return 5;
}

int ThreadsModel::rowCount(const QModelIndex &parent) const {
	Q_UNUSED(parent);
	return items_.size();
}

void ThreadsModel::addThread(const IThread::pointer &thread, bool current) {
	beginInsertRows(QModelIndex(), rowCount(), rowCount());

	const Item item = {
		thread, current
	};
	items_.push_back(item);
	endInsertRows();
}

void ThreadsModel::clear() {
	beginRemoveRows(QModelIndex(), 0, rowCount());
	items_.clear();
	endRemoveRows();
}
