
#ifndef FLOAT_X_H_20151108
#define FLOAT_X_H_20151108

#include "Types.h"
#include <QValidator>
#include <cmath>
#include <limits>
#include <type_traits>

template <class Float>
Float readFloat(const QString& strInput, bool& ok);

template <class Float>
class FloatXValidator : public QValidator {
public:
    explicit FloatXValidator(QObject* parent = nullptr) : QValidator(parent) {}
	QValidator::State validate(QString& input, int&) const override;
};

template <class Float>
QString formatFloat(Float value);

// Only class, nothing about sign
enum class FloatValueClass {
	Zero,
	Normal,
	Infinity,
	Denormal,
	PseudoDenormal,
	QNaN,
	SNaN,
	Unsupported
};

FloatValueClass floatType(edb::value32 value);
FloatValueClass floatType(edb::value64 value);
FloatValueClass floatType(edb::value80 value);

// This will work not only for floats, but also for integers
template <class T>
std::size_t maxPrintedLength() {
	using Limits = std::numeric_limits<T>;

	static bool isInteger              = Limits::is_integer;
	static const int mantissaChars     = isInteger ? 1+Limits::digits10 : Limits::max_digits10;
	static const int signChars         = std::is_signed<T>::value;
	static const int expSignChars      = !isInteger;
	static const int decimalPointChars = !isInteger;
	static const int expSymbol         = !isInteger; // 'e' for floating-point value in scientific format
	static const int expMaxWidth       = isInteger ? 0 : std::ceil(std::log10(Limits::max_exponent10));
	static const int maxWidth          = signChars + mantissaChars + decimalPointChars + expSymbol + expSignChars + expMaxWidth;

	return maxWidth;
}

#endif
