/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef UTIL_INTEGER_H_2020227_
#define UTIL_INTEGER_H_2020227_

#include "Value.h"
#include "util/Error.h"
#include <type_traits>

enum class NumberDisplayMode {
	Hex,
	Signed,
	Unsigned,
	Float
};

namespace util {

template <class T>
constexpr std::make_unsigned_t<T> to_unsigned(T x) {
	return x;
}

template <class T>
QString format_int(T value, NumberDisplayMode mode) {
	switch (mode) {
	case NumberDisplayMode::Hex:
		return value.toHexString();
	case NumberDisplayMode::Signed:
		return value.signedToString();
	case NumberDisplayMode::Unsigned:
		return value.unsignedToString();
	default:
		EDB_PRINT_AND_DIE("Unexpected integer display mode ", static_cast<long>(mode));
	}
}

}

#endif
