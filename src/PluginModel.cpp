/*
 * Copyright (C) 2014 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "PluginModel.h"

#include <QtAlgorithms>

/**
 * @brief Constructor for the PluginModel class.
 *
 * @param parent The parent QObject for this model.
 */
PluginModel::PluginModel(QObject *parent)
	: QAbstractItemModel(parent) {
}

/**
 * @brief Calculates the index of the item in the model based on the given row and column.
 *
 * @param row The row of the item.
 * @param column The column of the item.
 * @param parent The parent index of the item.
 * @return The QModelIndex representing the item.
 */
QModelIndex PluginModel::index(int row, int column, const QModelIndex &parent) const {

	if (row >= rowCount(parent) || column >= columnCount(parent)) {
		return QModelIndex();
	}

	if (row >= 0) {
		return createIndex(row, column, const_cast<Item *>(&items_[row]));
	}

	return createIndex(row, column);
}

/**
 * @brief Returns the parent index of the given index.
 *
 * @param index The index for which to find the parent.
 * @return The parent index of the given index.
 */
QModelIndex PluginModel::parent(const QModelIndex & /*index*/) const {
	return QModelIndex();
}

/**
 * @brief Returns the data stored under the given role for the item referred to by the index.
 *
 * @param index The index of the item.
 * @param role The role for which to return data.
 * @return The data stored under the given role for the item referred to by the index.
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
 * @brief Returns the data for the given role and section in the header with the specified orientation.
 *
 * @param section The section of the header.
 * @param orientation The orientation of the header (horizontal or vertical).
 * @param role The role for which to return data.
 * @return The data for the given role and section in the header with the specified orientation.
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
 * @brief Returns the number of columns in the model.
 *
 * @param parent The parent index.
 * @return The number of columns in the model.
 */
int PluginModel::columnCount(const QModelIndex & /*parent*/) const {
	return 4;
}

/**
 * @brief Returns the number of rows in the model.
 *
 * @param parent The parent index.
 * @return The number of rows in the model.
 */
int PluginModel::rowCount(const QModelIndex & /*parent*/) const {
	return static_cast<int>(items_.size());
}

/**
 * @brief Adds a new plugin to the model.
 *
 * @param filename The filename of the plugin.
 * @param plugin The name of the plugin.
 * @param author The author of the plugin.
 * @param url The URL of the plugin.
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
 * @brief Clears all plugins from the model.
 */
void PluginModel::clear() {
	beginResetModel();
	items_.clear();
	endResetModel();
}
