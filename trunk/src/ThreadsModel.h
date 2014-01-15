/*
Copyright (C) 2014 - 2014 Evan Teran
                          eteran@alum.rit.edu

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

#ifndef THREADS_MODEL_H_
#define THREADS_MODEL_H_

#include <QAbstractItemModel>
#include <QVector>
#include "Types.h"

class ThreadsModel : public QAbstractItemModel {
	Q_OBJECT

public:
	struct Item {
		edb::tid_t tid;
		bool       current;
	};

public:
	ThreadsModel(QObject *parent = 0);
	virtual ~ThreadsModel();

public:
	virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
	virtual QModelIndex parent(const QModelIndex &index) const;
	virtual QVariant data(const QModelIndex &index, int role) const;
	virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
	virtual int columnCount(const QModelIndex &parent = QModelIndex()) const;
	virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;

public:
	void addThread(edb::tid_t tid, bool current);
	void clear();

private:
	QVector<Item> items_;
};

#endif
