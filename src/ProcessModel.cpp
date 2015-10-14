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

#include "ProcessModel.h"
#include "IProcess.h"
#include <QtAlgorithms>

ProcessModel::ProcessModel(QObject *parent) : QAbstractItemModel(parent) {
}

ProcessModel::~ProcessModel() {
}

QModelIndex ProcessModel::index(int row, int column, const QModelIndex &parent) const {
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

QModelIndex ProcessModel::parent(const QModelIndex &index) const {
	Q_UNUSED(index);
	return QModelIndex();
}

QVariant ProcessModel::data(const QModelIndex &index, int role) const {

	if(index.isValid()) {

		const Item &item = items_[index.row()];

		if(role == Qt::DisplayRole) {
			switch(index.column()) {
			case 0:
				return item.pid;
			case 1:
				return item.user;
			case 2:
				return item.name;
			}
		} else if(role == Qt::UserRole) {
			return item.pid;
		}
	}

	return QVariant();
}

QVariant ProcessModel::headerData(int section, Qt::Orientation orientation, int role) const {

	if(role == Qt::DisplayRole && orientation == Qt::Horizontal) {
		switch(section) {
		case 0:
			return tr("PID");
		case 1:
			return tr("UID");
		case 2:
			return tr("Name");
		}
	}

	return QVariant();
}

int ProcessModel::columnCount(const QModelIndex &parent) const {
	Q_UNUSED(parent);
	return 3;
}

int ProcessModel::rowCount(const QModelIndex &parent) const {
	Q_UNUSED(parent);
	return items_.size();
}

void ProcessModel::addProcess(const IProcess::pointer &process) {
	beginInsertRows(QModelIndex(), rowCount(), rowCount());

	const Item item = {
		process->pid(), process->uid(), process->user(), process->name()
	};
	items_.push_back(item);
	endInsertRows();
}

void ProcessModel::clear() {
	beginRemoveRows(QModelIndex(), 0, rowCount());
	items_.clear();
	endRemoveRows();
}
