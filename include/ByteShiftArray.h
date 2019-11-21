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

#ifndef BYTE_SHIFT_ARRAY_H_20060825_
#define BYTE_SHIFT_ARRAY_H_20060825_

#include "API.h"
#include <QVector>
#include <cstddef>

class EDB_EXPORT ByteShiftArray {
public:
	explicit ByteShiftArray(int size);
	ByteShiftArray(const ByteShiftArray &) = delete;
	ByteShiftArray &operator=(const ByteShiftArray &) = delete;

public:
	ByteShiftArray &shl();
	ByteShiftArray &shr();
	void clear();
	void swap(ByteShiftArray &other);

public:
	ByteShiftArray &operator<<(uint8_t x);

public:
	int size() const;

public:
	uint8_t &operator[](std::size_t i);
	uint8_t operator[](std::size_t i) const;
	const uint8_t *data() const;

private:
	QVector<uint8_t> data_;
	int maxSize_;
};

#endif
