/*
Copyright (C) 2014 - 2015 Evan Teran
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

#ifndef PLUGIN_MODEL_H_
#define PLUGIN_MODEL_H_

#include <QAbstractItemModel>
#include <QVector>
#include <QString>

class PluginModel : public QAbstractItemModel {
	Q_OBJECT

public:
	struct Item {
		QString filename;
		QString plugin;
		QString author;
		QString url;
	};

public:
	PluginModel(QObject *parent = 0);
	virtual ~PluginModel();

public:
	virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
	virtual QModelIndex parent(const QModelIndex &index) const;
	virtual QVariant data(const QModelIndex &index, int role) const;
	virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
	virtual int columnCount(const QModelIndex &parent = QModelIndex()) const;
	virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;

public:
	void addPlugin(const QString &filename, const QString &plugin, const QString &author, const QString &url);
	void clear();

private:
	QVector<Item> items_;
};

#endif
