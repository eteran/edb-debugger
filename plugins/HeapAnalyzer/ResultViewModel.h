/*
Copyright (C) 2006 - 2015 Evan Teran
                          evan.teran@gmail.com

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
	QVariant data(const QModelIndex &index, int role) const override;
	QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
	QModelIndex parent(const QModelIndex &index) const override;
	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	int columnCount(const QModelIndex &parent = QModelIndex()) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

public:
	void addResult(const Result &r);
	void clearResults();
	void setPointerData(const QModelIndex &index, const std::vector<edb::address_t> &pointers);

public:
	const QVector<Result> &results() const { return results_; }

private:
	QVector<Result> results_;
};

}

#endif
