
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

template <typename T>
constexpr typename std::make_unsigned<T>::type to_unsigned(T x) {
	return x;
}

template <typename T>
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
