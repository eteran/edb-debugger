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
	QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
	QModelIndex parent(const QModelIndex &index) const override;
	QVariant data(const QModelIndex &index, int role) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
	int columnCount(const QModelIndex &parent = QModelIndex()) const override;
	int rowCount(const QModelIndex &parent = QModelIndex()) const override;

public:
	void addPlugin(const QString &filename, const QString &plugin, const QString &author, const QString &url);
	void clear();

private:
	QVector<Item> items_;
};

#endif
