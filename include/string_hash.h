/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef STRING_HASH_H_20110823_
#define STRING_HASH_H_20110823_

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace edb {
namespace detail {

template <std::size_t N, std::size_t Index = N - 1>
constexpr uint64_t string_hash(const char (&array)[N]) {
	if constexpr (Index == 0) {
		return 0;
	} else {
		return string_hash<N, Index - 1>(array) | ((array[Index - 1] & 0xffull) << (8 * (Index - 1)));
	}
}

}

template <std::size_t N, class = std::enable_if_t<N <= 9>>
constexpr uint64_t string_hash(const char (&array)[N]) {
	return detail::string_hash(array);
}

}

#endif
