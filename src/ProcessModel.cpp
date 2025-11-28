/*
 * Copyright (C) 2014 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ProcessModel.h"
#include "IProcess.h"

#include <QtAlgorithms>

ProcessModel::ProcessModel(QObject *parent)
	: QAbstractItemModel(parent) {
}

QModelIndex ProcessModel::index(int row, int column, const QModelIndex &parent) const {
	Q_UNUSED(parent)

	if (row >= rowCount(parent) || column >= columnCount(parent)) {
		return QModelIndex();
	}

	if (row >= 0) {
		return createIndex(row, column, const_cast<Item *>(&items_[row]));
	}

	return createIndex(row, column);
}

QModelIndex ProcessModel::parent(const QModelIndex &index) const {
	Q_UNUSED(index)
	return QModelIndex();
}

QVariant ProcessModel::data(const QModelIndex &index, int role) const {

	if (index.isValid()) {

		const Item &item = items_[index.row()];

		if (role == Qt::DisplayRole) {
			switch (index.column()) {
			case 0:
				return QVariant::fromValue(item.pid);
			case 1:
				return item.user;
			case 2:
				return item.name;
			}
		} else if (role == Qt::UserRole) {
			return QVariant::fromValue(item.pid);
		}
	}

	return QVariant();
}

QVariant ProcessModel::headerData(int section, Qt::Orientation orientation, int role) const {

	if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
		switch (section) {
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
	Q_UNUSED(parent)
	return 3;
}

int ProcessModel::rowCount(const QModelIndex &parent) const {
	Q_UNUSED(parent)
	return items_.size();
}

void ProcessModel::addProcess(const std::shared_ptr<IProcess> &process) {
	beginInsertRows(QModelIndex(), rowCount(), rowCount());

	const Item item = {
		process->pid(),
		process->uid(),
		process->user(),
		process->name(),
	};

	items_.push_back(item);
	endInsertRows();
}

void ProcessModel::clear() {
	beginResetModel();
	items_.clear();
	endResetModel();
}
