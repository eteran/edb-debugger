/*
 * Copyright (C) 2014 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PROCESS_MODEL_H_20191119_
#define PROCESS_MODEL_H_20191119_

#include "OSTypes.h"

#include <QAbstractItemModel>
#include <QString>
#include <QVector>

#include <memory>

class IProcess;

class ProcessModel final : public QAbstractItemModel {
	Q_OBJECT

public:
	struct Item {
		edb::pid_t pid;
		edb::uid_t uid;
		QString user;
		QString name;
	};

public:
	explicit ProcessModel(QObject *parent = nullptr);
	~ProcessModel() override = default;

public:
	[[nodiscard]] QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
	[[nodiscard]] QModelIndex parent(const QModelIndex &index) const override;
	[[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
	[[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
	[[nodiscard]] int columnCount(const QModelIndex &parent = QModelIndex()) const override;
	[[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;

public:
	void addProcess(const std::shared_ptr<IProcess> &process);
	void clear();

private:
	QVector<Item> items_;
};

#endif
