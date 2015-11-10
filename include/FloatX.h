#ifndef FLOAT_X_H_20151108
#define FLOAT_X_H_20151108

#include <type_traits>
#include <QValidator>
#include <Types.h>

template<typename Float>
Float readFloat(const QString& strInput,bool& ok);

template<typename Float>
class FloatXValidator : public QValidator
{
public:
	explicit FloatXValidator(QObject* parent=0) : QValidator(parent) {}
	virtual QValidator::State validate(QString& input, int&) const override;
};

template<typename Float>
QString formatFloat(Float value);

// Only class, nothing about sign
enum class FloatValueClass
{
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
template<typename T>
std::size_t maxPrintedLength()
{
	using Limits=std::numeric_limits<T>;
	static bool isInteger=Limits::is_integer;
	static const int mantissaChars=isInteger ? 1+Limits::digits10 : Limits::max_digits10;
	static const int signChars=1;
	static const int expSignChars=!isInteger;
	static const int decimalPointChars=!isInteger;
	static const int expSymbol=!isInteger; // 'e' for floating-point value in scientific format
	static const int expMaxWidth=isInteger ? 0 : std::ceil(std::log10(Limits::max_exponent10));
	static const int maxWidth=signChars+mantissaChars+decimalPointChars+expSymbol+expSignChars+expMaxWidth;

	return maxWidth;
}

#endif
