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

#ifndef SHIFT_BUFFER_20121017_H_
#define SHIFT_BUFFER_20121017_H_

#include <QtGlobal>

template <size_t N>
class ShiftBuffer {
public:
	ShiftBuffer() {
		qFill(buffer_, buffer_ + N, 0);
	}
	
	~ShiftBuffer() {
	}
	
	ShiftBuffer(const ShiftBuffer &other) {
		qCopy(other.buffer_, other.buffer_ + N, buffer_);
	}
	
	ShiftBuffer &operator=(const ShiftBuffer &rhs) {
		ShiftBuffer(rhs).swap(*this);
		return *this;
	}

public:
	void shl() {
		for(size_t i = 0; i < (N - 1); ++i) {
			buffer_[i] = buffer_[i + 1];
		}
		buffer_[N - 1] = 0;
	}
	
	void shr() {
		for(size_t i = N - 1; i > 0; --i) {
			buffer_[i] = buffer_[i - 1];
		}
		buffer_[0] = 0;
	}
	
public:
	size_t size() const {
		return N;
	}
	
public:
	const quint8 *begin() const { return buffer_; }
	const quint8 *end() const   { return buffer_ + N; }
	quint8 *begin()             { return buffer_; }
	quint8 *end()               { return buffer_ + N; }

public:
	quint8 operator[](size_t n) const {
		return buffer_[n];
	}
	
	quint8 &operator[](size_t n) {
		return buffer_[n];
	}
	
public:
	void swap(ShiftBuffer &other) {
		for(size_t i = 0; i < N; ++i) {
			qSwap(buffer_[i], other.buffer_[i]);
		}
	}
	
private:
	quint8 buffer_[N];
};

#endif
