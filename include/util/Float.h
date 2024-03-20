
#ifndef UTIL_FLOAT_H_2020227_
#define UTIL_FLOAT_H_2020227_

#include "FloatX.h"
#include <cerrno>
#include <optional>
#include <string>
#include <type_traits>

namespace util {

template <class Float>
std::optional<Float> full_string_to_float(const std::string &s) {

	static_assert(
		std::is_same_v<Float, float> ||
			std::is_same_v<Float, double> ||
			std::is_same_v<Float, long double>,
		"Floating-point type not supported by this function");

	// NOTE: Don't use std::istringstream: it doesn't support hexfloat.
	//       Don't use std::sto{f,d,ld} either: they throw std::out_of_range on denormals.
	//       Only std::strto{f,d,ld} are sane, for some definitions of sane...
	const char *const str = s.c_str();
	char *end;
	Float value;
	errno = 0;

	if constexpr (std::is_same_v<Float, float>) {
		value = std::strtof(str, &end);
	} else if constexpr (std::is_same_v<Float, double>) {
		value = std::strtod(str, &end);
	} else if constexpr (std::is_same_v<Float, long double>) {
		value = std::strtold(str, &end);
	}

	if (errno) {
		if ((errno == ERANGE && (value == 0 || std::isinf(value))) || errno != ERANGE) {
			return {};
		}
	}

	if (end == str + s.size()) {
		return value;
	}

	return {};
}

}

#endif
