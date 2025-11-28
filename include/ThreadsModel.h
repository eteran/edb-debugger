/*
 * Copyright (C) 2014 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef THREADS_MODEL_H_20191119_
#define THREADS_MODEL_H_20191119_

#include "API.h"
#include <QAbstractItemModel>
#include <QVector>
#include <memory>

class IThread;

class EDB_EXPORT ThreadsModel final : public QAbstractItemModel {
	Q_OBJECT

public:
	struct Item {
		std::shared_ptr<IThread> thread;
		bool current;
	};

public:
	ThreadsModel(QObject *parent = nullptr);
	~ThreadsModel() override = default;

public:
	[[nodiscard]] QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
	[[nodiscard]] QModelIndex parent(const QModelIndex &index) const override;
	[[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
	[[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
	[[nodiscard]] int columnCount(const QModelIndex &parent = QModelIndex()) const override;
	[[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;

public:
	void addThread(const std::shared_ptr<IThread> &thread, bool current);
	void clear();

private:
	QVector<Item> items_;
};

#endif
