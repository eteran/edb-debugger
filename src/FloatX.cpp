/*
Copyright (C) 2017 - 2017 Evan Teran
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

#include "FloatX.h"
#include "util/Float.h"

#include <iomanip>
#include <sstream>

#if defined(HAVE_DOUBLE_CONVERSION)
#include <double-conversion/double-conversion.h>
#endif

#if defined(HAVE_GDTOA)
#include <gdtoa-desktop.h>
#endif

#ifdef _MSC_VER
extern "C" EDB_EXPORT void __fastcall float64_to_float80(const void *src, void *dest);

// NOTE(eteran): this thin wrapper function make look pointless... and it REALLY does.
// However, I could not get plugins to be able to see the functions defined in .asm files
// unless I wrapped them in a concrete function like this. It's dumb, but it works
void convert_real64_to_real80(const void *src, void *dst) {
	float64_to_float80(src, dst);
}
#endif

namespace {

template <class T>
struct SpecialValues;

template <>
struct SpecialValues<double> {
	static constexpr std::array<std::uint8_t, 8> positiveInf{{0, 0, 0, 0, 0, 0, 0xf0, 0x7f}};
	static constexpr std::array<std::uint8_t, 8> negativeInf{{0, 0, 0, 0, 0, 0, 0xf0, 0xff}};
	static constexpr std::array<std::uint8_t, 8> positiveSNaN{{0, 0, 0, 0, 0, 0, 0xfc, 0x7f}};
	static constexpr std::array<std::uint8_t, 8> negativeSNaN{{0, 0, 0, 0, 0, 0, 0xfc, 0xff}};
	static constexpr std::array<std::uint8_t, 8> positiveQNaN{{0, 0, 0, 0, 0, 0, 0xf8, 0x7f}};
	static constexpr std::array<std::uint8_t, 8> negativeQNaN{{0, 0, 0, 0, 0, 0, 0xf8, 0xff}};
};

template <>
struct SpecialValues<float> {
	static constexpr std::array<std::uint8_t, 4> positiveInf{{0, 0, 0x80, 0x7f}};
	static constexpr std::array<std::uint8_t, 4> negativeInf{{0, 0, 0x80, 0xff}};
	static constexpr std::array<std::uint8_t, 4> positiveSNaN{{0, 0, 0xe0, 0x7f}};
	static constexpr std::array<std::uint8_t, 4> negativeSNaN{{0, 0, 0xe0, 0xff}};
	static constexpr std::array<std::uint8_t, 4> positiveQNaN{{0, 0, 0xc0, 0x7f}};
	static constexpr std::array<std::uint8_t, 4> negativeQNaN{{0, 0, 0xc0, 0xff}};
};

#ifndef _MSC_VER
#if defined(EDB_X86) || defined(EDB_X86_64)
template <>
struct SpecialValues<long double> {

	static_assert(std::numeric_limits<long double>::digits == 64 && std::numeric_limits<long double>::max_exponent == 16384,
				  "Expected to have x87 80-bit long double");

	static constexpr std::array<std::uint8_t, 16> positiveInf{{0, 0, 0, 0, 0, 0, 0, 0x80, 0xff, 0x7f, 0, 0, 0, 0, 0, 0}};
	static constexpr std::array<std::uint8_t, 16> negativeInf{{0, 0, 0, 0, 0, 0, 0, 0x80, 0xff, 0xff, 0, 0, 0, 0, 0, 0}};
	static constexpr std::array<std::uint8_t, 16> positiveSNaN{{0, 0, 0, 0, 0, 0, 0, 0x90, 0xff, 0x7f, 0, 0, 0, 0, 0, 0}};
	static constexpr std::array<std::uint8_t, 16> negativeSNaN{{0, 0, 0, 0, 0, 0, 0, 0x90, 0xff, 0xff, 0, 0, 0, 0, 0, 0}};
	static constexpr std::array<std::uint8_t, 16> positiveQNaN{{0, 0, 0, 0, 0, 0, 0, 0xc0, 0xff, 0x7f, 0, 0, 0, 0, 0, 0}};
	static constexpr std::array<std::uint8_t, 16> negativeQNaN{{0, 0, 0, 0, 0, 0, 0, 0xc0, 0xff, 0xff, 0, 0, 0, 0, 0, 0}};
};
#endif
#endif

constexpr std::array<std::uint8_t, 4> SpecialValues<float>::positiveInf;
constexpr std::array<std::uint8_t, 4> SpecialValues<float>::negativeInf;
constexpr std::array<std::uint8_t, 4> SpecialValues<float>::positiveSNaN;
constexpr std::array<std::uint8_t, 4> SpecialValues<float>::negativeSNaN;
constexpr std::array<std::uint8_t, 4> SpecialValues<float>::positiveQNaN;
constexpr std::array<std::uint8_t, 4> SpecialValues<float>::negativeQNaN;

constexpr std::array<std::uint8_t, 8> SpecialValues<double>::positiveInf;
constexpr std::array<std::uint8_t, 8> SpecialValues<double>::negativeInf;
constexpr std::array<std::uint8_t, 8> SpecialValues<double>::positiveSNaN;
constexpr std::array<std::uint8_t, 8> SpecialValues<double>::negativeSNaN;
constexpr std::array<std::uint8_t, 8> SpecialValues<double>::positiveQNaN;
constexpr std::array<std::uint8_t, 8> SpecialValues<double>::negativeQNaN;

#ifndef _MSC_VER
#if defined(EDB_X86) || defined(EDB_X86_64)
constexpr std::array<std::uint8_t, 16> SpecialValues<long double>::positiveInf;
constexpr std::array<std::uint8_t, 16> SpecialValues<long double>::negativeInf;
constexpr std::array<std::uint8_t, 16> SpecialValues<long double>::positiveSNaN;
constexpr std::array<std::uint8_t, 16> SpecialValues<long double>::negativeSNaN;
constexpr std::array<std::uint8_t, 16> SpecialValues<long double>::positiveQNaN;
constexpr std::array<std::uint8_t, 16> SpecialValues<long double>::negativeQNaN;
#endif
#endif

float to_real(edb::value32 value) {
	float result;
	std::memcpy(&result, &value, sizeof(result));
	return result;
}

double to_real(edb::value64 value) {
	double result;
	std::memcpy(&result, &value, sizeof(result));
	return result;
}

long double to_real(edb::value80 value) {
	return value.toFloatValue();
}

template <unsigned MantissaLength, typename FloatHolder>
FloatValueClass ieee_classify(FloatHolder value) {

	constexpr auto ExpLength = 8 * sizeof(value) - MantissaLength - 1;
	constexpr auto ExpMax    = (1u << ExpLength) - 1;
	constexpr auto QNaN_mask = 1ull << (MantissaLength - 1);

	const auto mantissa = value & ((1ull << MantissaLength) - 1);
	const auto exponent = (value >> MantissaLength) & ExpMax;

	if (exponent == ExpMax) {
		if (mantissa == 0u) {
			return FloatValueClass::Infinity; // |S|11..11|00..00|
		} else if (mantissa & QNaN_mask) {
			return FloatValueClass::QNaN; // |S|11..11|1XX..XX|
		} else {
			return FloatValueClass::SNaN; // |S|11..11|0XX..XX|
		}
	} else if (exponent == 0u) {
		if (mantissa == 0u) {
			return FloatValueClass::Zero; // |S|00..00|00..00|
		} else {
			return FloatValueClass::Denormal; // |S|00..00|XX..XX|
		}
	} else {
		return FloatValueClass::Normal;
	}
}

#if defined(HAVE_GDTOA)
/*
 * gdtoa_g_?fmt functions do generally a good job at formatting the numbers in
 * a form close to that specified in ECMAScript specification (actually the
 * spec even references this implementation in 7.1.12.1/note3). There are two
 * issues though with the function, one small and one bigger.
 *
 * The small issue is that this function prints numbers x such that 1e-5<|x|<1
 * without leading zeros (contrary to the spec). It's easy to fix up, and it's
 * the first thing the following function does.
 *
 * The bigger issue is that the specification prescribes to print large numbers
 * smaller than 1e21 in fixed-point format, and append zeros(!) instead of
 * actual digits present in the closest representable integer to the base part.
 *
 * This can be quite a problem for our users, because it can give a false sense
 * of precision. E.g., consider the number 1.2345678912345678e20. Closest
 * representable IEEE 754 binary64 number is 123456789123456778240. Next
 * representable is 123456789123456794624. Yet gdtoa_g_dfmt (following
 * ECMAScript) formats it as 123456789123456780000, which, having so many
 * trailing zeros, misleads the user into thinking that all the digits are
 * significant (the user may think that e.g. 123456789123456781000 or even
 * 123456789123456780001 are representable too).
 *
 * The following function tries to ensure that such situations never occur: it
 * takes numeric_limits::digits10 digits as the maximum length of integers (or
 * maximum value of exponent+1 for fractions) for fixed-point format, and if the
 * number formatted into 'buffer' is larger than that, it converts the result
 * into exponential format, removing any trailing zeros.
 *
 * Note that in principle, we could use gdtoa_gdtoa directly and format the
 * number ourselves. But this would result in even more logic to 1) prepare
 * arguments, 2) actually do the formatting; total of both might be just as
 * convoluted as the current post-processing logic.
 */
const char *fixup_g_Yfmt(char *buffer, int digits10) {

	const size_t len = std::strlen(buffer);
	const char x0    = buffer[0];
	const char x1    = buffer[1];

	if (x0 == '.' || (x0 == '-' && x1 == '.')) {
		// ".235" or "-.235" forms are unreadable, so insert leading zero
		const size_t posToInsert = x0 == '.' ? 0 : 1;
		// Give space for the zero: move the remaining line with terminating zero to the right
		std::memmove(buffer + posToInsert + 1, buffer + posToInsert, len + 1 - posToInsert);
		buffer[posToInsert] = '0';
		return buffer;
	}

	// We want exponential format for numbers which are too imprecise for fixed-point format.
	// If we find a number with more than digits10 digits before point, we must fix it.
	int digitCount = 0;
	int pointPos   = -1;
	for (int i = 0; i < static_cast<int>(len); ++i) {
		const char c = buffer[i];
		// If it's already in exponential format, it's fine, just return it
		if (c == 'e') {
			return buffer;
		}

		if (c == '.') {
			pointPos = i;
			continue;
		} else if ('0' <= c && c <= '9') {
			++digitCount;
		}
	}

	// If point wasn't found, assume it's at the end of the number
	if (pointPos < 0) {
		pointPos = len;
	}

	const int signChars = buffer[0] == '-';
	const int exp       = pointPos - signChars - 1;
	if (exp + 1 > digits10) {

		// Yes, the format is too precise for actual value, need to shift the
		// point to the second position and append e+XX to the resulting string.
		char *const buf = buffer + signChars;

		// NOTE: don't attempt to replace this loop with memmove: you'll get
		// confused trying to work out size of data to move.
		// The original string may contain a point, may not contain any. In the
		// former case we must move everything including the null terminator. In
		// the latter case only the chunk up to the original point needs moving.
		const int lenWithNull = len + 1;

		char next = buf[1];
		for (int i = 0; i < lenWithNull - signChars; ++i) {
			// Avoid writing the point; after this, there's no more need to shift
			if (next == '.') {
				break;
			}
			std::swap(next, buf[i + 1]);
		}

		buf[1] = '.';

		// Now after all the mess with present/absent point, it's better to
		// re-calculate length of the current buffer content
		auto len = std::strlen(buf);

		// Remove trailing zeros
		while (buf[len - 1] == '0') {
			--len;
		}

		// Append the exponent
		buf[len]     = 'e';
		buf[len + 1] = '+';
		buf[len + 2] = exp / 10 + '0';
		buf[len + 3] = exp % 10 + '0';
		buf[len + 4] = 0;
	}

	return buffer;
}
#endif

}

template <class Float>
Float read_float(const QString &input, bool &ok) {

	ok = false;
	const QString str(input.toLower().trimmed());
	if (const auto value = util::full_string_to_float<Float>(str.toStdString())) {
		ok = true;
		return *value;
	}

	// OK, so either it is invalid/unfinished, or it's some special value
	// We still do want the user to be able to enter common special values
	Float value;
	if (str == "+snan" || str == "snan") {
		std::memcpy(&value, &SpecialValues<Float>::positiveSNaN, sizeof(value));
	} else if (str == "-snan") {
		std::memcpy(&value, &SpecialValues<Float>::negativeSNaN, sizeof(value));
	} else if (str == "+qnan" || str == "qnan" || str == "nan") {
		std::memcpy(&value, &SpecialValues<Float>::positiveQNaN, sizeof(value));
	} else if (str == "-qnan") {
		std::memcpy(&value, &SpecialValues<Float>::negativeQNaN, sizeof(value));
	} else if (str == "+inf" || str == "inf") {
		std::memcpy(&value, &SpecialValues<Float>::positiveInf, sizeof(value));
	} else if (str == "-inf") {
		std::memcpy(&value, &SpecialValues<Float>::negativeInf, sizeof(value));
	} else {
		return 0;
	}

	ok = true;
	return value;
}

template EDB_EXPORT float read_float<float>(const QString &input, bool &ok);
template EDB_EXPORT double read_float<double>(const QString &input, bool &ok);

#ifndef _MSC_VER
#if defined(EDB_X86) || defined(EDB_X86_64)
template long double read_float<long double>(const QString &input, bool &ok);
#endif
#endif

FloatValueClass float_type(edb::value32 value) {
	return ieee_classify<23>(value);
}

FloatValueClass float_type(edb::value64 value) {
	return ieee_classify<52>(value);
}

FloatValueClass float_type(edb::value80 value) {
	constexpr auto MantissaLength = 64;
	constexpr auto ExpLength      = 8 * sizeof(value) - MantissaLength - 1;
	constexpr auto IntegerBitOnly = 1ull << (MantissaLength - 1);
	constexpr auto QNaN_mask      = 3ull << (MantissaLength - 2);
	constexpr auto ExpMax         = (1u << ExpLength) - 1;

	const auto exponent      = value.exponent();
	const auto mantissa      = value.mantissa();
	const bool integerBitSet = mantissa & IntegerBitOnly;

	// This is almost as ieee_classify, but also takes integer bit (not present in
	// IEEE754 format) into account to detect unsupported values
	if (exponent == ExpMax) {
		if (mantissa == IntegerBitOnly) {
			return FloatValueClass::Infinity; // |S|11..11|1.000..0|
		} else if ((mantissa & QNaN_mask) == QNaN_mask) {
			return FloatValueClass::QNaN; // |S|11..11|1.1XX..X|
		} else if ((mantissa & QNaN_mask) == IntegerBitOnly) {
			return FloatValueClass::SNaN; // |S|11..11|1.0XX..X|
		} else {
			return FloatValueClass::Unsupported; // all exp bits set, but integer bit reset - pseudo-NaN/Inf
		}
	} else if (exponent == 0u) {
		if (mantissa == 0u) {
			return FloatValueClass::Zero; // |S|00..00|00..00|
		} else {
			if (!integerBitSet) {
				return FloatValueClass::Denormal; // |S|00..00|0.XXXX..X|
			} else {
				return FloatValueClass::PseudoDenormal; // |S|00..00|1.XXXX..X|
			}
		}
	} else {
		if (integerBitSet) {
			return FloatValueClass::Normal;
		} else {
			return FloatValueClass::Unsupported; // integer bit reset but exp is as if normal - unnormal
		}
	}
}

template <typename Float>
QValidator::State FloatXValidator<Float>::validate(QString &input, int &) const {

	if (input.isEmpty()) {
		return QValidator::Intermediate;
	}

	// The input may be in hex format. std::istream doesn't support extraction
	// of hexfloat, but std::strto[f,d,ld] do. (see wg21.link/lwg2381)
	if (const auto v = util::full_string_to_float<Float>(input.toStdString())) {
		return QValidator::Acceptable;
	}

	// OK, so we failed to read it or it is unfinished. Let's check whether it's intermediate or invalid.
	QRegExp basicFormat("[+-]?[0-9]*\\.?[0-9]*(e([+-]?[0-9]*)?)?");
	QRegExp specialFormat("[+-]?[sq]?nan|[+-]?inf", Qt::CaseInsensitive);
	QRegExp hexfloatFormat("[+-]?0x[0-9a-f]*\\.?[0-9a-f]*(p([+-]?[0-9]*)?)?", Qt::CaseInsensitive);
	QRegExp specialFormatUnfinished("[+-]?[sq]?(n(an?)?)?|[+-]?(i(nf?)?)?", Qt::CaseInsensitive);

	if (hexfloatFormat.exactMatch(input)) {
		return QValidator::Intermediate;
	}

	if (basicFormat.exactMatch(input)) {
		return QValidator::Intermediate;
	}

	if (specialFormat.exactMatch(input)) {
		return QValidator::Acceptable;
	}

	if (specialFormatUnfinished.exactMatch(input)) {
		return QValidator::Intermediate;
	}

	// All possible options are exhausted, so consider the input invalid
	return QValidator::Invalid;
}

template QValidator::State FloatXValidator<float>::validate(QString &input, int &) const;
template QValidator::State FloatXValidator<double>::validate(QString &input, int &) const;
template QValidator::State FloatXValidator<long double>::validate(QString &input, int &) const;

template <typename Float>
EDB_EXPORT QString format_float(Float value) {

	const auto type    = float_type(value);
	QString specialStr = "???? ";

	switch (type) {
	case FloatValueClass::Zero:
		return value.negative() ? "-0.0" : "0.0";

	case FloatValueClass::PseudoDenormal:
		if (sizeof(value) >= 10) {
			Q_ASSERT(sizeof(value) == 10);

			// Convert to supported value as the CPU would. Otherwise glibc takes it wrong.
			const uint16_t exponent = value.negative() ? 0x8001 : 0x0001;

			// NOTE(eteran): hushes a warning from GCC >= 8.2
			auto ptr = reinterpret_cast<char *>(&value) + 8;

			std::memcpy(ptr, &exponent, sizeof(exponent));
		}
		// fall through
	case FloatValueClass::Normal:
	case FloatValueClass::Denormal: {
#ifdef HAVE_DOUBLE_CONVERSION
		constexpr bool isDouble = std::is_same<Float, edb::value64>::value;
		constexpr bool isFloat  = std::is_same<Float, edb::value32>::value;
		if (isDouble || isFloat) {
			using namespace double_conversion;

			char buffer[64];
			DoubleToStringConverter conv(DoubleToStringConverter::EMIT_POSITIVE_EXPONENT_SIGN, "inf", "nan", 'e', -4, isDouble ? 15 : 6, 0, 0);

			StringBuilder builder(buffer, sizeof(buffer));
			bool ret = false;

			// NOTE(eteran): we get a warning here about out of bounds memory reads, but it functionally can't happen at runtime
			// if we had constexpr if, we could avoid the warning
			if (isDouble) {
				double d;
				std::memcpy(&d, &value, sizeof(d));
				ret = conv.ToShortest(d, &builder);
			} else {
				// isFloat
				float f;
				std::memcpy(&f, &value, sizeof(f));
				ret = conv.ToShortestSingle(f, &builder);
			}

			if (ret && builder.Finalize()) {
				const QString result = buffer;
				if (result.size() == 1 && result[0].isDigit()) {
					return result + ".0"; // avoid printing small whole numbers as integers
				}

				return result;
			}
		}
#endif
#if defined(HAVE_GDTOA)
		if (std::is_same<Float, edb::value80>::value) {
			char buffer[64] = {};
			gdtoa_g_xfmt(buffer, &value, -1, sizeof buffer);
			fixup_g_Yfmt(buffer, std::numeric_limits<long double>::digits10);

			const QString result = buffer;
			if (result.size() == 1 && result[0].isDigit()) {
				return result + ".0"; // avoid printing small whole numbers as integers
			}

			return result;
		}
#endif
		std::ostringstream ss;
		ss << std::setprecision(std::numeric_limits<decltype(to_real(value))>::max_digits10) << to_real(value);

		const auto result = QString::fromStdString(ss.str());
		if (result.size() == 1 && result[0].isDigit()) {
			return result + ".0"; // avoid printing small whole numbers as integers
		}

		return result;
	}
	case FloatValueClass::Infinity:
		return QString(value.negative() ? "-" : "+") + "INF";
	case FloatValueClass::QNaN:
		specialStr = QString(value.negative() ? "-" : "+") + "QNAN ";
		break;
	case FloatValueClass::SNaN:
		specialStr = QString(value.negative() ? "-" : "+") + "SNAN ";
		break;
	case FloatValueClass::Unsupported:
		specialStr = QString(value.negative() ? "-" : "+") + "BAD ";
		break;
	}

	// If we are here, then the value is special
	auto hexStr = value.toHexString();

	const QChar groupSeparator(' ');
	if (hexStr.size() > 8) {
		hexStr.insert(hexStr.size() - 8, groupSeparator);
	}

	if (hexStr.size() > 16 + 1) { // +1 to take into account already inserted first separator
		hexStr.insert(hexStr.size() - 16 - 1, groupSeparator);
	}

	return specialStr + hexStr;
}

template EDB_EXPORT QString format_float<edb::value32>(edb::value32);
template EDB_EXPORT QString format_float<edb::value64>(edb::value64);
template EDB_EXPORT QString format_float<edb::value80>(edb::value80);
