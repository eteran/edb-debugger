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

#ifndef BYTESHIFTARRAY_20060825_H_
#define BYTESHIFTARRAY_20060825_H_

#include "API.h"

#include <cstddef>
#include <QVector>

class EDB_EXPORT ByteShiftArray {
	Q_DISABLE_COPY(ByteShiftArray)
public:
	explicit ByteShiftArray(int size);

public:
	ByteShiftArray &shl();
	ByteShiftArray &shr();
	void clear();
	void swap(ByteShiftArray &other);

public:
	ByteShiftArray &operator<<(quint8 x);

public:
	int size() const;

public:
	quint8 &operator[](std::size_t i);
	quint8 operator[](std::size_t i) const;
	const quint8 *data() const;

private:
	QVector<quint8> data_;
	int             max_size_;
};

#endif

