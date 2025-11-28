/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef BYTE_SHIFT_ARRAY_H_20060825_
#define BYTE_SHIFT_ARRAY_H_20060825_

#include "API.h"
#include <QVector>
#include <cstddef>
#include <cstdint>

class EDB_EXPORT ByteShiftArray {
public:
	explicit ByteShiftArray(int size);
	ByteShiftArray(const ByteShiftArray &)            = delete;
	ByteShiftArray &operator=(const ByteShiftArray &) = delete;

public:
	ByteShiftArray &shl();
	ByteShiftArray &shr();
	void clear();
	void swap(ByteShiftArray &other);

public:
	ByteShiftArray &operator<<(uint8_t x);

public:
	[[nodiscard]] int size() const;

public:
	[[nodiscard]] uint8_t &operator[](std::size_t i);
	[[nodiscard]] uint8_t operator[](std::size_t i) const;
	[[nodiscard]] const uint8_t *data() const;

private:
	QVector<uint8_t> data_;
	int maxSize_;
};

#endif
