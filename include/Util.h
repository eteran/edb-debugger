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

#ifndef UTIL_20061126_H_
#define UTIL_20061126_H_

#include "FloatX.h"
#include <QString>
#include <algorithm>
#include <array>
#include <boost/optional.hpp>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <type_traits>

enum class NumberDisplayMode {
	Hex, 
	Signed, 
	Unsigned, 
	Float
};

namespace util {

// Used to interconvert between abstract enum defined in an interface and actual enumerators in implementation
template <typename AbstractEnum, typename ConcreteEnum>
class AbstractEnumData {
	AbstractEnum data;
public:
	AbstractEnumData(AbstractEnum a) : data(a) {
	}
	
	AbstractEnumData(ConcreteEnum c) : data(static_cast<AbstractEnum>(c)) {
	}
	
	operator AbstractEnum() const { return data; }
	operator ConcreteEnum() const { return static_cast<ConcreteEnum>(data); }
};

template <typename T>
constexpr typename std::make_unsigned<T>::type to_unsigned(T x) {
	return x;
}

//------------------------------------------------------------------------------
// Name: percentage
// Desc: calculates how much of a multi-region byte search we have completed
//------------------------------------------------------------------------------
template <class N1, class N2, class N3, class N4>
int percentage(N1 regions_finished, N2 regions_total, N3 bytes_done, N4 bytes_total) {

	// how much percent does each region account for?
	const auto region_step = 1.0f / static_cast<float>(regions_total) * 100.0f;

	// how many regions are done?
	const float regions_complete = region_step * regions_finished;

	// how much of the current region is done?
	const float region_percent = region_step * static_cast<float>(bytes_done) / static_cast<float>(bytes_total);

	return static_cast<int>(regions_complete + region_percent);
}

//------------------------------------------------------------------------------
// Name: percentage
// Desc: calculates how much of a single-region byte search we have completed
//------------------------------------------------------------------------------
template <class N1, class N2>
int percentage(N1 bytes_done, N2 bytes_total) {
	return percentage(0, 1, bytes_done, bytes_total);
}

inline void markMemory(void *memory, std::size_t size) {

	auto p = reinterpret_cast<char *>(memory);
	// Fill memory with 0xbad1bad1 marker
	for (std::size_t i = 0; i < size; ++i) {
		p[i] = i & 1 ? 0xba : 0xd1;
	}
}

template <typename T, typename... Tail>
constexpr auto make_array(T head, Tail... tail) -> std::array<T, 1 + sizeof...(Tail)> {
	return std::array<T, 1 + sizeof...(Tail)>{ head, tail... };
}

template <typename Container, typename Element>
bool contains(const Container &container, const Element &element) {
	return std::find(std::begin(container), std::end(container), element) != std::end(container);
}

template <typename Container, typename Pred>
bool contains_if(const Container &container, Pred pred) {
	return std::find_if(std::begin(container), std::end(container), pred) != std::end(container);
}

template <typename Stream>
void print(Stream &str) {
	str << "\n";
}

template <typename Stream, typename Arg0, typename... Args>
void print(Stream &str, const Arg0 &arg0, const Args &... args) {
	str << arg0;
	print(str, args...);
}

#define EDB_PRINT_AND_DIE(...) \
do {                                                                                                    \
	util::print(std::cerr, __FILE__, ":", __LINE__, ": ", Q_FUNC_INFO, ": Fatal error: ", __VA_ARGS__); \
	std::abort();                                                                                       \
} while(0)


template <typename T>
QString formatInt(T value, NumberDisplayMode mode) {
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


template <typename Float>
boost::optional<Float> fullStringToFloat(const std::string &s) {

	static_assert(
		std::is_same<Float, float>::value  || 
		std::is_same<Float, double>::value ||
		std::is_same<Float, long double>::value,
		"Floating-point type not supported by this function");
		
	// NOTE: Don't use std::istringstream: it doesn't support hexfloat.
	//       Don't use std::sto{f,d,ld} either: they throw std::out_of_range on denormals.
	//       Only std::strto{f,d,ld} are sane, for some definitions of sane...
	const char *const str = s.c_str();
	char *end;
	Float value;
	errno = 0;
	
	if (std::is_same<Float, float>::value) {
		value = std::strtof(str, &end);
	} else if (std::is_same<Float, double>::value) {
		value = std::strtod(str, &end);
	} else {
		value = std::strtold(str, &end);
	}
	
	if (errno) {
		if ((errno == ERANGE && (value == 0 || std::isinf(value))) || errno != ERANGE) {
			return boost::none;
		}
	}

	if (end == str + s.size()) {
		return value;
	}

	return boost::none;
}

template <class T, size_t N, class U = T>
constexpr void shl(std::array<T, N> &buffer, U value = T()) {
	static_assert (std::is_convertible<T, U>::value, "U must be convertable to the type contained in the array!");
	std::rotate(buffer.begin(), buffer.begin() + 1, buffer.end());
	buffer[N - 1] = value;
}

template <class T, size_t N, class U = T>
constexpr void shr(std::array<T, N> &buffer, U value = T()) {
	static_assert (std::is_convertible<T, U>::value, "U must be convertable to the type contained in the array!");
	std::rotate(buffer.rbegin(), buffer.rbegin() + 1, buffer.rend());
	buffer[0] = value;
}

template <class T, size_t N>
constexpr void rol(std::array<T, N> &buffer) {
	std::rotate(buffer.begin(), buffer.begin() + 1, buffer.end());
}

template <class T, size_t N>
constexpr void ror(std::array<T, N> &buffer) {
	std::rotate(buffer.rbegin(), buffer.rbegin() + 1, buffer.rend());
}

}

#endif
