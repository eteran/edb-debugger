
#ifndef UTIL_FLOAT_H_2020227_
#define UTIL_FLOAT_H_2020227_

#include "FloatX.h"
#include <boost/optional.hpp>
#include <cerrno>
#include <string>
#include <type_traits>

namespace util {

template <typename Float>
boost::optional<Float> full_string_to_float(const std::string &s) {

	static_assert(
		std::is_same<Float, float>::value ||
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

}

#endif
