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

#include "ByteShiftArray.h"

//------------------------------------------------------------------------------
// Name: ByteShiftArray
// Desc: constructor
//------------------------------------------------------------------------------
ByteShiftArray::ByteShiftArray(int size) : max_size_(size) {
}

//------------------------------------------------------------------------------
// Name: swap
// Desc:
//------------------------------------------------------------------------------
void ByteShiftArray::swap(ByteShiftArray &other) {
	qSwap(data_,     other.data_);
	qSwap(max_size_, other.max_size_);
}

//------------------------------------------------------------------------------
// Name: shl
// Desc: shifts data left one byte and shifts in a 0
//------------------------------------------------------------------------------
ByteShiftArray &ByteShiftArray::shl() {
	
	if(data_.size() == max_size_) {
		for(int i = 1; i < data_.size(); ++i) {
			data_[i - 1] = data_[i];
		}
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
	if(data_.size() == max_size_) {
		for(int i = 0; i < data_.size() - 1; ++i) {
			data_[i + 1] = data_[i];
		}
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
quint8 &ByteShiftArray::operator[](std::size_t i) {
	return data_[i];
}

//------------------------------------------------------------------------------
// Name: operator[]
// Desc: returns and r-value version of an element in the byte array
//------------------------------------------------------------------------------
quint8 ByteShiftArray::operator[](std::size_t i) const {
	return data_[i];
}

//------------------------------------------------------------------------------
// Name: data
// Desc: returns a read only pointer to the data this byte array holds
//------------------------------------------------------------------------------
const quint8 *ByteShiftArray::data() const {
	return &data_[0];
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
ByteShiftArray &ByteShiftArray::operator<<(quint8 x) {
	shl();
	data_.back() = x;
	return *this;
}
