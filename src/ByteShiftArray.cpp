/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ByteShiftArray.h"
#include <climits>

/**
 * @brief Constructor for the ByteShiftArray class.
 * @param size The maximum size of the array.
 */
ByteShiftArray::ByteShiftArray(int size)
	: maxSize_(size) {
}

/**
 * @brief Swaps the contents of this array with another array.
 * @param other The other array to swap with.
 */
void ByteShiftArray::swap(ByteShiftArray &other) noexcept {
	using std::swap;
	swap(data_, other.data_);
	swap(maxSize_, other.maxSize_);
}

/**
 * @brief Shifts the data left by one byte, inserting a 0 at the end.
 * @return A reference to this array.
 */
ByteShiftArray &ByteShiftArray::shl() {

	if (data_.size() == maxSize_) {
		std::rotate(data_.begin(), data_.begin() + 1, data_.end());
		data_.back() = 0;
	} else {
		data_.push_back(0);
	}
	return *this;
}

/**
 * @brief Shifts the data right by one byte and shifts in a 0.
 * @return A reference to this array.
 */
ByteShiftArray &ByteShiftArray::shr() {
	if (data_.size() == maxSize_) {
		std::rotate(data_.rbegin(), data_.rbegin() + 1, data_.rend());
		data_.first() = 0;
	} else {
		data_.push_front(0);
	}
	return *this;
}

/**
 * @brief Returns the size of this byte array.
 * @return The size of the array.
 */
int ByteShiftArray::size() const {
	return data_.size();
}

/**
 * @brief Returns a reference to the element at the specified index.
 * @param i The index of the element to return.
 * @return A reference to the element at the specified index.
 */
uint8_t &ByteShiftArray::operator[](std::size_t i) {
	Q_ASSERT(i < static_cast<std::size_t>(data_.size()));
	return data_[static_cast<int>(i)];
}

/**
 * @brief Returns the byte at the specified index.
 * @param i The index of the byte to return.
 * @return The byte at the specified index.
 */
uint8_t ByteShiftArray::operator[](std::size_t i) const {
	Q_ASSERT(i < static_cast<std::size_t>(data_.size()));
	return data_[static_cast<int>(i)];
}

/**
 * @brief Returns a pointer to the data this byte array holds.
 * @return A pointer to the data.
 */
const uint8_t *ByteShiftArray::data() const {
	return data_.data();
}

/**
 * @brief Clears the byte array by filling it with zeros.
 */
void ByteShiftArray::clear() {
	data_.fill(0);
}

/**
 * @brief Shifts the data left by one byte and inserts the specified byte at the end.
 * @param x The byte to insert at the end.
 * @return A reference to this array.
 */
ByteShiftArray &ByteShiftArray::operator<<(uint8_t x) {
	shl();
	data_.back() = x;
	return *this;
}
