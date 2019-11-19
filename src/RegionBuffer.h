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

#ifndef REGION_BUFFER_H_20101111_
#define REGION_BUFFER_H_20101111_

#include "IRegion.h"

#include <QIODevice>

#include <memory>

class IRegion;

class RegionBuffer final : public QIODevice {
	Q_OBJECT
public:
	explicit RegionBuffer(const std::shared_ptr<IRegion> &region);
	RegionBuffer(const std::shared_ptr<IRegion> &region, QObject *parent);

public:
	void setRegion(const std::shared_ptr<IRegion> &region);

public:
	qint64 readData(char *data, qint64 maxSize) override;
	qint64 writeData(const char *, qint64) override;
	qint64 size() const override { return region_ ? region_->size() : 0; }
	bool isSequential() const override { return false; }

private:
	std::shared_ptr<IRegion> region_;
};

#endif
