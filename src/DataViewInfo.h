/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DATA_VIEW_INFO_H_20100101_
#define DATA_VIEW_INFO_H_20100101_

#include <QtGlobal>
#include <memory>

class QHexView;
class RegionBuffer;
class IRegion;

class DataViewInfo {
public:
	explicit DataViewInfo(const std::shared_ptr<IRegion> &r);
	DataViewInfo();
	DataViewInfo(const DataViewInfo &)            = delete;
	DataViewInfo &operator=(const DataViewInfo &) = delete;
	~DataViewInfo()                               = default;

public:
	std::shared_ptr<IRegion> region;
	std::shared_ptr<QHexView> view;
	std::unique_ptr<RegionBuffer> stream;

public:
	void update();
};

#endif
