/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ByteShiftArray.h"
#include <climits>

//------------------------------------------------------------------------------
// Name: ByteShiftArray
// Desc: constructor
//------------------------------------------------------------------------------
ByteShiftArray::ByteShiftArray(int size)
	: maxSize_(size) {
}

//------------------------------------------------------------------------------
// Name: swap
// Desc:
//------------------------------------------------------------------------------
void ByteShiftArray::swap(ByteShiftArray &other) {
	using std::swap;
	swap(data_, other.data_);
	swap(maxSize_, other.maxSize_);
}

//------------------------------------------------------------------------------
// Name: shl
// Desc: shifts data left one byte and shifts in a 0
//------------------------------------------------------------------------------
ByteShiftArray &ByteShiftArray::shl() {

	if (data_.size() == maxSize_) {
		std::rotate(data_.begin(), data_.begin() + 1, data_.end());
		data_.back() = 0;
	} else {
		data_.push_back(0);
	}
	return *this;
}

//------------------------------------------------------------------------------
// Name: shr
// Desc: shifts data right one byte and shifts in a 0
//------------------------------------------------------------------------------
ByteShiftArray &ByteShiftArray::shr() {
	if (data_.size() == maxSize_) {
		std::rotate(data_.rbegin(), data_.rbegin() + 1, data_.rend());
		data_.first() = 0;
	} else {
		data_.push_front(0);
	}
	return *this;
}

//------------------------------------------------------------------------------
// Name: size
// Desc: returns size of this byte array
//------------------------------------------------------------------------------
int ByteShiftArray::size() const {
	return data_.size();
}

//------------------------------------------------------------------------------
// Name: operator[]
// Desc: returns and l-value version of an element in the byte array
//------------------------------------------------------------------------------
uint8_t &ByteShiftArray::operator[](std::size_t i) {
	Q_ASSERT(i < data_.size());
	return data_[static_cast<int>(i)];
}

//------------------------------------------------------------------------------
// Name: operator[]
// Desc: returns and r-value version of an element in the byte array
//------------------------------------------------------------------------------
uint8_t ByteShiftArray::operator[](std::size_t i) const {
	Q_ASSERT(i < data_.size());
	return data_[static_cast<int>(i)];
}

//------------------------------------------------------------------------------
// Name: data
// Desc: returns a read only pointer to the data this byte array holds
//------------------------------------------------------------------------------
const uint8_t *ByteShiftArray::data() const {
	return data_.data();
}

//------------------------------------------------------------------------------
// Name: clear
// Desc: zeros out the byte array
//------------------------------------------------------------------------------
void ByteShiftArray::clear() {
	data_.fill(0);
}

//------------------------------------------------------------------------------
// Name: operator<<
// Desc:
//------------------------------------------------------------------------------
ByteShiftArray &ByteShiftArray::operator<<(uint8_t x) {
	shl();
	data_.back() = x;
	return *this;
}
