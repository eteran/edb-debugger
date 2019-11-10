/*
Copyright (C) 2006 - 2015 Evan Teran
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
