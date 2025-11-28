/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef REGION_BUFFER_H_20101111_
#define REGION_BUFFER_H_20101111_

#include "IRegion.h"
#include <QIODevice>
#include <memory>

class IRegion;

class RegionBuffer final : public QIODevice {
	Q_OBJECT
public:
	explicit RegionBuffer(std::shared_ptr<IRegion> region);
	RegionBuffer(std::shared_ptr<IRegion> region, QObject *parent);

public:
	void setRegion(std::shared_ptr<IRegion> region);

public:
	qint64 readData(char *data, qint64 maxSize) override;
	qint64 writeData(const char *, qint64) override;
	[[nodiscard]] qint64 size() const override { return region_ ? region_->size() : 0; }
	[[nodiscard]] bool isSequential() const override { return false; }

private:
	std::shared_ptr<IRegion> region_;
};

#endif
