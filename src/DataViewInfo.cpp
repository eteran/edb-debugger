/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DataViewInfo.h"
#include "QHexView"
#include "RegionBuffer.h"

/**
 * @brief Constructs a DataViewInfo object with the specified memory region.
 *
 * @param r A shared pointer to the memory region associated with this DataViewInfo.
 */
DataViewInfo::DataViewInfo(const std::shared_ptr<IRegion> &r)
	: region(r), stream(std::make_unique<RegionBuffer>(r)) {
}

/**
 * @brief Updates the DataViewInfo object with the current memory region.
 */
void DataViewInfo::update() {

	Q_ASSERT(view);

	stream->setRegion(region);
	view->setAddressOffset(region->start());
	view->setData(stream.get());
}
