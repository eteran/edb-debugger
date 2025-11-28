/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef ROP_TOOL_RESULTS_MODEL_H_20191119_
#define ROP_TOOL_RESULTS_MODEL_H_20191119_

#include "Function.h"
#include "Types.h"
#include <QAbstractItemModel>
#include <QVector>

namespace ROPToolPlugin {

class ResultsModel : public QAbstractItemModel {
	Q_OBJECT
public:
	struct Result {
		edb::address_t address = 0;
		QString instruction;
		uint32_t role = 0x00;
	};

public:
	explicit ResultsModel(QObject *parent = nullptr);

public:
	[[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
	[[nodiscard]] QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
	[[nodiscard]] QModelIndex parent(const QModelIndex &index) const override;
	[[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	[[nodiscard]] int columnCount(const QModelIndex &parent = QModelIndex()) const override;
	[[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
	void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

public:
	void addResult(const Result &r);

public:
	[[nodiscard]] const QVector<Result> &results() const { return results_; }

private:
	QVector<Result> results_;
};

}

#endif
