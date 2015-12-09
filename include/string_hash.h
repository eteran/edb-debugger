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

#ifndef STRING_HASH_20110823_H_
#define STRING_HASH_20110823_H_

#include <cstdint>
#include <type_traits>

namespace edb {

namespace detail {

template<std::size_t N, std::size_t n>
constexpr typename std::enable_if<N<=9 && n==0,
uint64_t>::type string_hash(const char (&)[N]) { return 0; }

template<std::size_t N, std::size_t n=N-1>
constexpr typename std::enable_if<N<=9 && n!=0,
uint64_t>::type string_hash(const char (&array)[N]) {
	return string_hash<N,n-1>(array) | ((array[n-1]&0xffull)<<(8*(n-1)));
}

}

template<std::size_t N>
constexpr typename std::enable_if<N<=9,
uint64_t>::type string_hash(const char (&array)[N]) {
	return detail::string_hash(array);
}

}

#endif
