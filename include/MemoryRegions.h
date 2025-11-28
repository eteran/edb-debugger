/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MEMORY_REGIONS_H_20060501_
#define MEMORY_REGIONS_H_20060501_

#include "API.h"
#include "Types.h"
#include <QAbstractItemModel>
#include <QList>
#include <memory>

class IRegion;

class EDB_EXPORT MemoryRegions final : public QAbstractItemModel {
	Q_OBJECT

public:
	[[nodiscard]] QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
	[[nodiscard]] QModelIndex parent(const QModelIndex &index) const override;
	[[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
	[[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
	[[nodiscard]] int columnCount(const QModelIndex &parent = QModelIndex()) const override;
	[[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;

public:
	MemoryRegions();
	~MemoryRegions() override = default;

public:
	[[nodiscard]] std::shared_ptr<IRegion> findRegion(edb::address_t address) const;
	[[nodiscard]] const QList<std::shared_ptr<IRegion>> &regions() const { return regions_; }
	void clear();
	void sync();

private:
	QList<std::shared_ptr<IRegion>> regions_;
};

#endif
