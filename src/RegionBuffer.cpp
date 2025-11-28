/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "RegionBuffer.h"
#include "IDebugger.h"
#include "IProcess.h"
#include "edb.h"

//------------------------------------------------------------------------------
// Name: RegionBuffer
// Desc:
//------------------------------------------------------------------------------
RegionBuffer::RegionBuffer(std::shared_ptr<IRegion> region)
	: region_(std::move(region)) {

	setOpenMode(QIODevice::ReadOnly);
}

//------------------------------------------------------------------------------
// Name: RegionBuffer
// Desc:
//------------------------------------------------------------------------------
RegionBuffer::RegionBuffer(std::shared_ptr<IRegion> region, QObject *parent)
	: QIODevice(parent), region_(std::move(region)) {

	setOpenMode(QIODevice::ReadOnly);
}

//------------------------------------------------------------------------------
// Name: set_region
// Desc:
//------------------------------------------------------------------------------
void RegionBuffer::setRegion(std::shared_ptr<IRegion> region) {
	region_ = std::move(region);
	reset();
}

//------------------------------------------------------------------------------
// Name: readData
// Desc:
//------------------------------------------------------------------------------
qint64 RegionBuffer::readData(char *data, qint64 maxSize) {

	if (region_) {
		if (IProcess *process = edb::v1::debugger_core->process()) {
			const edb::address_t start = region_->start() + pos();
			const edb::address_t end   = region_->start() + region_->size();

			if (start + maxSize > end) {
				maxSize = end - start;
			}

			if (maxSize == 0) {
				return 0;
			}

			if (process->readBytes(start, data, maxSize)) {
				return maxSize;
			}

			return -1;
		}
	}

	return -1;
}

//------------------------------------------------------------------------------
// Name: writeData
// Desc:
//------------------------------------------------------------------------------
qint64 RegionBuffer::writeData(const char *, qint64) {
	return -1;
}
