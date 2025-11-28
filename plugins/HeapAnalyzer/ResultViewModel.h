/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef RESULT_VIEW_MODEL_H_20070419_
#define RESULT_VIEW_MODEL_H_20070419_

#include "Types.h"
#include <QAbstractItemModel>
#include <QVector>
#include <vector>

namespace HeapAnalyzerPlugin {

class ResultViewModel : public QAbstractItemModel {
	Q_OBJECT

public:
	struct Result {

		enum DataType {
			Unknown,
			Pointer,
			Png,
			Xpm,
			Bzip,
			Compress,
			Gzip,
			Ascii,
			Utf16
		};

		enum NodeType {
			Top,
			Free,
			Busy
		};

		edb::address_t address = 0;
		edb::address_t size    = 0;
		NodeType type;
		DataType dataType = Unknown;
		QString data;
		std::vector<edb::address_t> pointers;
	};

public:
	explicit ResultViewModel(QObject *parent = nullptr);

public:
	[[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
	[[nodiscard]] QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
	[[nodiscard]] QModelIndex parent(const QModelIndex &index) const override;
	[[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	[[nodiscard]] int columnCount(const QModelIndex &parent = QModelIndex()) const override;
	[[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

public:
	void addResult(const Result &r);
	void clearResults();
	void setPointerData(const QModelIndex &index, const std::vector<edb::address_t> &pointers);

public:
	[[nodiscard]] const QVector<Result> &results() const { return results_; }

private:
	QVector<Result> results_;
};

}

#endif
