/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef UTIL_H_20061126_
#define UTIL_H_20061126_

#include <cstddef>
#include <cstdint>

namespace util {

// Used to interconvert between abstract enum defined in an interface and actual enumerators in implementation
template <class AbstractEnum, class ConcreteEnum>
class AbstractEnumData {
	AbstractEnum data;

public:
	AbstractEnumData(AbstractEnum a)
		: data(a) {
	}

	AbstractEnumData(ConcreteEnum c)
		: data(static_cast<AbstractEnum>(c)) {
	}

	operator AbstractEnum() const { return data; }
	operator ConcreteEnum() const { return static_cast<ConcreteEnum>(data); }
};

inline void mark_memory(void *memory, std::size_t size) {

	auto p = reinterpret_cast<uint8_t *>(memory);

	// Fill memory with 0xbad1bad1 marker
	for (std::size_t i = 0; i < size; ++i) {
		p[i] = (i & 1) ? 0xba : 0xd1;
	}
}

}

#endif
