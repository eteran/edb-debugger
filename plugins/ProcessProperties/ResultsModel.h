/*
Copyright (C) 2006 - 2023 Evan Teran
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

#ifndef PROCESS_PROPERTIES_RESULTS_MODEL_H_20191119_
#define PROCESS_PROPERTIES_RESULTS_MODEL_H_20191119_

#include "Function.h"
#include "Types.h"
#include <QAbstractItemModel>
#include <QVector>

namespace ProcessPropertiesPlugin {

class ResultsModel : public QAbstractItemModel {
	Q_OBJECT

public:
	struct Result {
		edb::address_t address = 0;
		QString string;
		enum {
			Ascii,
			Utf8,
			Utf16,
			Utf32,
		} type;
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
