/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef RESULTS_MODEL_H_20070419_
#define RESULTS_MODEL_H_20070419_

#include "Function.h"
#include "Types.h"
#include <QAbstractItemModel>
#include <QVector>

namespace FunctionFinderPlugin {

class ResultsModel : public QAbstractItemModel {
	Q_OBJECT
public:
	struct Result {
		edb::address_t startAddress = 0;
		edb::address_t endAddress   = 0;
		size_t size                 = 0;
		int score                   = 0;
		Function::Type type         = Function::Type::Standard;
		QString symbol;
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
