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

#include <cstdint>
#include <array>
#include <algorithm>

template <size_t N>
class ShiftBuffer {
public:
	ShiftBuffer() {
		std::fill(buffer_.begin(), buffer_.end(), 0);
	}

	~ShiftBuffer()                                 = default;
	ShiftBuffer(const ShiftBuffer &other)          = default;
	ShiftBuffer &operator=(const ShiftBuffer &rhs) = default;

public:
	void shl() {
		std::rotate(buffer_.begin(), buffer_.begin() + 1, buffer_.end());
		buffer_[N - 1] = 0;
	}

	void shr() {
		std::rotate(buffer_.rbegin(), buffer_.rbegin() + 1, buffer_.rend());
		buffer_[0] = 0;
	}

public:
	size_t size() const {
		return N;
	}

public:
	const uint8_t *begin() const { return buffer_.begin(); }
	const uint8_t *end() const   { return buffer_.end(); }
	uint8_t *begin()             { return buffer_.begin(); }
	uint8_t *end()               { return buffer_.end(); }

public:
	const uint8_t *data() const   { return buffer_.data(); }
	uint8_t *data()               { return buffer_.data(); }

public:
	uint8_t operator[](size_t n) const {
		return buffer_[n];
	}

	uint8_t &operator[](size_t n) {
		return buffer_[n];
	}

private:
	std::array<uint8_t, N> buffer_;
};

#endif
