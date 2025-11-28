/*
 * Copyright (C) 2014 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ThreadsModel.h"
#include "IThread.h"
#include "edb.h"

#include <QtAlgorithms>

ThreadsModel::ThreadsModel(QObject *parent)
	: QAbstractItemModel(parent) {
}

QModelIndex ThreadsModel::index(int row, int column, const QModelIndex &parent) const {
	Q_UNUSED(parent)

	if (row >= rowCount(parent) || column >= columnCount(parent)) {
		return QModelIndex();
	}

	if (row >= 0) {
		return createIndex(row, column, const_cast<Item *>(&items_[row]));
	}

	return createIndex(row, column);
}

QModelIndex ThreadsModel::parent(const QModelIndex &index) const {
	Q_UNUSED(index)
	return QModelIndex();
}

QVariant ThreadsModel::data(const QModelIndex &index, int role) const {

	if (index.isValid()) {

		const Item &item = items_[index.row()];

		if (role == Qt::DisplayRole) {
			switch (index.column()) {
			case 0:
				if (item.current) {
					return tr("*%1").arg(item.thread->tid());
				}

				return QVariant::fromValue(item.thread->tid());

			case 1:
				return item.thread->priority();
			case 2: {
				const QString default_region_name;
				const QString symname = edb::v1::find_function_symbol(item.thread->instructionPointer(), default_region_name);

				if (!symname.isEmpty()) {
					return QStringLiteral("%1 <%2>").arg(edb::v1::format_pointer(item.thread->instructionPointer()), symname);
				}

				return QStringLiteral("%1").arg(edb::v1::format_pointer(item.thread->instructionPointer()));
			}
			case 3:
				return item.thread->runState();
			case 4:
				return item.thread->name();
			}
		} else if (role == Qt::UserRole) {
			return QVariant::fromValue(item.thread->tid());
		}
	}

	return QVariant();
}

QVariant ThreadsModel::headerData(int section, Qt::Orientation orientation, int role) const {

	if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
		switch (section) {
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
	Q_UNUSED(parent)
	return 5;
}

int ThreadsModel::rowCount(const QModelIndex &parent) const {
	Q_UNUSED(parent)
	return items_.size();
}

void ThreadsModel::addThread(const std::shared_ptr<IThread> &thread, bool current) {
	beginInsertRows(QModelIndex(), rowCount(), rowCount());

	const Item item = {
		thread,
		current,
	};

	items_.push_back(item);
	endInsertRows();
}

void ThreadsModel::clear() {
	beginResetModel();
	items_.clear();
	endResetModel();
}
