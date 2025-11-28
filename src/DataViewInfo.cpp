/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DataViewInfo.h"
#include "QHexView"
#include "RegionBuffer.h"

//------------------------------------------------------------------------------
// Name: DataViewInfo
// Desc:
//------------------------------------------------------------------------------
DataViewInfo::DataViewInfo(const std::shared_ptr<IRegion> &r)
	: region(r), stream(std::make_unique<RegionBuffer>(r)) {
}

DataViewInfo::DataViewInfo()
	: DataViewInfo(nullptr) {
}

//------------------------------------------------------------------------------
// Name: update
// Desc:
//------------------------------------------------------------------------------
void DataViewInfo::update() {

	Q_ASSERT(view);

	stream->setRegion(region);
	view->setAddressOffset(region->start());
	view->setData(stream.get());
}
