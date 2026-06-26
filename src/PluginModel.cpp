/*
 * Copyright (C) 2014 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "PluginModel.h"

#include <QtAlgorithms>

/**
 * @brief
 */
PluginModel::PluginModel(QObject *parent)
	: QAbstractItemModel(parent) {
}

/**
 * @brief
 */
QModelIndex PluginModel::index(int row, int column, const QModelIndex &parent) const {
	Q_UNUSED(parent)

	if (row >= rowCount(parent) || column >= columnCount(parent)) {
		return QModelIndex();
	}

	if (row >= 0) {
		return createIndex(row, column, const_cast<Item *>(&items_[row]));
	}

	return createIndex(row, column);
}

/**
 * @brief
 */
QModelIndex PluginModel::parent(const QModelIndex &index) const {
	Q_UNUSED(index)
	return QModelIndex();
}

/**
 * @brief
 */
QVariant PluginModel::data(const QModelIndex &index, int role) const {

	if (index.isValid()) {

		const Item &item = items_[index.row()];

		if (role == Qt::DisplayRole) {
			switch (index.column()) {
			case 0:
				return item.filename;
			case 1:
				return item.plugin;
			case 2:
				return item.author;
			case 3:
				return item.url;
			}
		}
	}

	return QVariant();
}

/**
 * @brief
 */
QVariant PluginModel::headerData(int section, Qt::Orientation orientation, int role) const {

	if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
		switch (section) {
		case 0:
			return tr("File Name");
		case 1:
			return tr("Plugin Name");
		case 2:
			return tr("Author");
		case 3:
			return tr("Website");
		}
	}

	return QVariant();
}

/**
 * @brief
 */
int PluginModel::columnCount(const QModelIndex &/*parent*/) const {
	return 4;
}

/**
 * @brief
 */
int PluginModel::rowCount(const QModelIndex &/*parent*/) const {
	return static_cast<int>(items_.size());
}

/**
 * @brief
 */
void PluginModel::addPlugin(const QString &filename, const QString &plugin, const QString &author, const QString &url) {
	beginInsertRows(QModelIndex(), rowCount(), rowCount());

	const Item item = {
		filename,
		plugin,
		author,
		url,
	};

	items_.push_back(item);
	endInsertRows();
}

/**
 * @brief
 */
void PluginModel::clear() {
	beginResetModel();
	items_.clear();
	endResetModel();
}
