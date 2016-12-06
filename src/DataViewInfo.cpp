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

//------------------------------------------------------------------------------
// Name: DataViewInfo
// Desc:
//------------------------------------------------------------------------------
DataViewInfo::DataViewInfo(const IRegion::pointer &r) : region(r), stream(new RegionBuffer(r)) {
}

//------------------------------------------------------------------------------
// Name: ~DataViewInfo
// Desc:
//------------------------------------------------------------------------------
DataViewInfo::~DataViewInfo() {
	delete stream;
}

//------------------------------------------------------------------------------
// Name: update
// Desc:
//------------------------------------------------------------------------------
void DataViewInfo::update() {

	Q_ASSERT(stream);
	Q_ASSERT(view);

	stream->set_region(region);
	view->setAddressOffset(region->start());
	view->setData(stream);
}

