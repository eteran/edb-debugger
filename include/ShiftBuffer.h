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
#include <algorithm>

template <size_t N>
class ShiftBuffer {
public:
	ShiftBuffer() {
		std::fill(buffer_, buffer_ + N, 0);
	}

	~ShiftBuffer() = default;

	ShiftBuffer(const ShiftBuffer &other) {
		std::copy(other.buffer_, other.buffer_ + N, buffer_);
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
	const uint8_t *begin() const { return buffer_; }
	const uint8_t *end() const   { return buffer_ + N; }
	uint8_t *begin()             { return buffer_; }
	uint8_t *end()               { return buffer_ + N; }

public:
	uint8_t operator[](size_t n) const {
		return buffer_[n];
	}

	uint8_t &operator[](size_t n) {
		return buffer_[n];
	}

public:
	void swap(ShiftBuffer &other) {
		for(size_t i = 0; i < N; ++i) {
			std::swap(buffer_[i], other.buffer_[i]);
		}
	}

private:
	uint8_t buffer_[N];
};

#endif
