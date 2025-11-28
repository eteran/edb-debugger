/*
 * Copyright (C) 2014 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PLUGIN_MODEL_H_20191119_
#define PLUGIN_MODEL_H_20191119_

#include <QAbstractItemModel>
#include <QString>
#include <QVector>

class PluginModel final : public QAbstractItemModel {
	Q_OBJECT

public:
	struct Item {
		QString filename;
		QString plugin;
		QString author;
		QString url;
	};

public:
	explicit PluginModel(QObject *parent = nullptr);
	~PluginModel() override = default;

public:
	[[nodiscard]] QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
	[[nodiscard]] QModelIndex parent(const QModelIndex &index) const override;
	[[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
	[[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
	[[nodiscard]] int columnCount(const QModelIndex &parent = QModelIndex()) const override;
	[[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;

public:
	void addPlugin(const QString &filename, const QString &plugin, const QString &author, const QString &url);
	void clear();

private:
	QVector<Item> items_;
};

#endif
