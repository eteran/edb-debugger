
#ifndef VALUE_H_
#define VALUE_H_

#include "API.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <vector>

#include <QString>
#include <QVariant>

class Register;

#ifdef _MSC_VER
extern "C" EDB_EXPORT void __fastcall long_double_to_double(const void* src, double* dest);
#endif

namespace edb {
namespace detail {

static constexpr int LargeSizedValueElementWidth = 64;

template <int N>
struct sized_uint {};

template <> struct sized_uint<8>  { using type = std::uint8_t; };
template <> struct sized_uint<16> { using type = std::uint16_t; };
template <> struct sized_uint<32> { using type = std::uint32_t; };
template <> struct sized_uint<64> { using type = std::uint64_t; };

template <int ElementWidth, int ElementCount>
class ValueBase {
	static_assert(ElementCount > 0, "ValueBase::value_ must be non-empty");

protected:
	using ValueType = std::array<typename sized_uint<ElementWidth>::type, ElementCount>;
	ValueType value_;

	template <class Data>
	explicit ValueBase(const Data& data, std::size_t offset = 0) {
		static_assert(sizeof(Data) >= sizeof(ValueType), "ValueBase can only be constructed from large enough variable");
#if defined __GNUG__ && __GNUC__ >= 5 || !defined __GNUG__ || defined __clang__ && __clang_major__*100+__clang_minor__>=306
		static_assert(std::is_trivially_copyable<Data>::value, "ValueBase can only be constructed from trivially copiable data");
#endif

		assert(sizeof(Data) - offset >= sizeof(ValueType)); // check bounds, this can't be done at compile time

		auto dataStart = reinterpret_cast<const char*>(&data);
		std::memcpy(&value_, dataStart + offset, sizeof(value_));
	}

	template <class SmallData>
	void copyZeroExtended(const SmallData& data) {
		static_assert(sizeof(SmallData) <= sizeof(ValueType), "It doesn't make sense to expand a larger type into a smaller type");

		auto dataStart = reinterpret_cast<const char*>(&data);
		auto target    = reinterpret_cast<char*>(&value_);

		std::memcpy(target, dataStart, sizeof(data));
		std::memset(target + sizeof(data), 0, sizeof(value_) - sizeof(data));
	}

	ValueBase() = default;

public:
	QString toHexString() const {
		std::ostringstream ss;

		for(auto it = value_.rbegin(); it != value_.rend(); ++it) {
			ss << std::setw(sizeof(*it) * 2) << std::setfill('0') << std::hex << +*it; // + to prevent printing uint8_t as a character
		}

		return QString::fromStdString(ss.str());
	}

	ValueType& value() { return value_; }
	const ValueType& value() const { return value_; }
	bool operator==(const ValueBase& other) const { return value_ == other.value_; }
	bool operator!=(const ValueBase& other) const { return !(*this == other); }
};

template <int N>
class SizedValue : public ValueBase<N,1> {
	static_assert((N % 8) == 0, "SizedValue must have multiple of 8 bits in size");

public:
	using InnerValueType = typename sized_uint<N>::type;

	SizedValue() = default;

	template <class Data, typename = typename std::enable_if<!std::is_floating_point<Data>::value && !std::is_integral<Data>::value>::type>
	explicit SizedValue (const Data& data, std::size_t offset) : ValueBase<N,1>(data,offset) {
		static_assert(sizeof(SizedValue) * 8 == N, "Size is broken!");
	}

	template <class Data, typename = typename std::enable_if<!std::is_floating_point<Data>::value && !std::is_integral<Data>::value>::type>
	explicit SizedValue (const Data& data) : ValueBase<N,1>(data, 0) {
		static_assert(sizeof(SizedValue) * 8 == N, "Size is broken!");
	}

	template <class Float, typename dummy = void, typename check = typename std::enable_if<std::is_floating_point<Float>::value>::type>
	explicit SizedValue(Float floatVal) {
		this->value_[0] = floatVal;
	}

	template <class Integer, typename = typename std::enable_if<std::is_integral<Integer>::value>::type>
	SizedValue(Integer integer) {
		this->value_[0] = integer;
	}

	SizedValue(const Register&) = delete;

	template <class SmallData>
	static SizedValue fromZeroExtended(const SmallData& data) {
		SizedValue created;
		created.copyZeroExtended(data);
		return created;
	}

	static SizedValue fromString(const QString& str, bool* ok = nullptr, int base = 10, bool Signed = false) {

		const qulonglong v = Signed ? str.toLongLong(ok, base) : str.toULongLong(ok, base);

		if(ok && !*ok) {
			return SizedValue(0);
		}

		// Check that the result fits into InnerValueType
		SizedValue result(v);
		if(result == v) {
			return result;
		}

		if(ok) {
			*ok = false;
		}

		return SizedValue(0);
	}

	static SizedValue fromHexString(const QString& str, bool* ok = nullptr)    { return fromString(str, ok, 16); }
	static SizedValue fromSignedString(const QString& str, bool* ok = nullptr) { return fromString(str, ok, 10, true); }
	static SizedValue fromCString(const QString& str, bool* ok = nullptr)      { return fromString(str, ok, 0); }

	operator InnerValueType() const { return this->value_[0]; }

	QVariant toQVariant() const { return QVariant::fromValue(this->value_[0]); }

	SizedValue signExtended(std::size_t valueLength) const {

		SizedValue result(*this);

		if(valueLength == sizeof(*this)) {
			return result;
		}

		if(this->value_[0] & (1ull << (valueLength * 8 - 1))) {
			result = -1ll;
			std::memcpy(&result, this, valueLength);
		}

		return result;
	}

	QString toString() const { return QString("%1").arg(this->value_[0]); }
	QString unsignedToString() const { return toString(); }
	QString signedToString() const { return QString("%1").arg(typename std::make_signed<InnerValueType>::type(this->value_[0])); }

	using ValueBase<N,1>::operator==;
	using ValueBase<N,1>::operator!=;

	template<class RHS> typename std::enable_if<std::is_integral<RHS>::value, bool>::type operator == (RHS rhs) const { return this->value_[0] == static_cast<InnerValueType>(rhs); }
	template<class RHS> typename std::enable_if<std::is_integral<RHS>::value, bool>::type operator != (RHS rhs) const { return this->value_[0] != static_cast<InnerValueType>(rhs); }
	template<class RHS> typename std::enable_if<std::is_integral<RHS>::value, bool>::type operator >  (RHS rhs) const { return this->value_[0] >  rhs; }
	template<class RHS> typename std::enable_if<std::is_integral<RHS>::value, bool>::type operator <  (RHS rhs) const { return this->value_[0] <  rhs; }
	template<class RHS> typename std::enable_if<std::is_integral<RHS>::value, bool>::type operator >= (RHS rhs) const { return this->value_[0] >= rhs; }
	template<class RHS> typename std::enable_if<std::is_integral<RHS>::value, bool>::type operator <= (RHS rhs) const { return this->value_[0] <= rhs; }
	template<class RHS> typename std::enable_if<std::is_integral<RHS>::value, SizedValue>::type operator + (RHS rhs) const { return SizedValue(this->value_[0] + rhs); }
	template<class RHS> typename std::enable_if<std::is_integral<RHS>::value, SizedValue>::type operator - (RHS rhs) const { return SizedValue(this->value_[0] - rhs); }
	template<class RHS> typename std::enable_if<std::is_integral<RHS>::value, SizedValue>::type operator & (RHS rhs) const { return SizedValue(this->value_[0] & rhs); }
	template<class RHS> typename std::enable_if<std::is_integral<RHS>::value, SizedValue>::type operator % (RHS rhs) const { return SizedValue(this->value_[0] % rhs); }
	template<class RHS> typename std::enable_if<std::is_integral<RHS>::value, SizedValue>::type operator += (RHS rhs) { this->value_[0] += rhs; return *this; }
	template<class RHS> typename std::enable_if<std::is_integral<RHS>::value, SizedValue>::type operator -= (RHS rhs) { this->value_[0] -= rhs; return *this; }
	template<class RHS> typename std::enable_if<std::is_integral<RHS>::value, SizedValue>::type operator ^= (RHS rhs) { this->value_[0] ^= rhs; return *this; }
	template<class RHS> typename std::enable_if<std::is_integral<RHS>::value, SizedValue>::type operator &= (RHS rhs) { this->value_[0] &= rhs; return *this; }
	template<class RHS> typename std::enable_if<std::is_integral<RHS>::value, SizedValue>::type operator |= (RHS rhs) { this->value_[0] |= rhs; return *this; }
	template<class RHS> typename std::enable_if<std::is_integral<RHS>::value, SizedValue>::type operator >> (RHS rhs) const { return SizedValue(this->value_[0] >> rhs); }
	template<class RHS> typename std::enable_if<std::is_integral<RHS>::value, SizedValue>::type operator << (RHS rhs) const { return SizedValue(this->value_[0] << rhs); }

	bool operator >  (SizedValue other) const { return this->value_[0] >  other.value_[0]; }
	bool operator <  (SizedValue other) const { return this->value_[0] <  other.value_[0]; }
	bool operator >= (SizedValue other) const { return this->value_[0] >= other.value_[0]; }
	bool operator <= (SizedValue other) const { return this->value_[0] <= other.value_[0]; }
	SizedValue operator + (const SizedValue& other) const { return SizedValue(this->value_[0] + other.value_[0]); }
	SizedValue operator - (const SizedValue& other) const { return SizedValue(this->value_[0] - other.value_[0]); }
	SizedValue operator += (SizedValue other) { this->value_[0] += other.value_[0]; return *this; }
	SizedValue operator -= (SizedValue other) { this->value_[0] -= other.value_[0]; return *this; }
	SizedValue operator ^= (SizedValue other) { this->value_[0] ^= other.value_[0]; return *this; }
	SizedValue operator &= (SizedValue other) { this->value_[0] &= other.value_[0]; return *this; }
	SizedValue operator |= (SizedValue other) { this->value_[0] |= other.value_[0]; return *this; }
	SizedValue operator <<=(SizedValue other) { this->value_[0] <<=other.value_[0]; return *this; }
	SizedValue operator >>=(SizedValue other) { this->value_[0] >>=other.value_[0]; return *this; }
	SizedValue operator *= (SizedValue other) { this->value_[0] *= other.value_[0]; return *this; }
	SizedValue operator /= (SizedValue other) { this->value_[0] /= other.value_[0]; return *this; }
	SizedValue operator %= (SizedValue other) { this->value_[0] %= other.value_[0]; return *this; }
	SizedValue operator ++ (int) { SizedValue copy(*this); ++this->value_[0]; return copy; }
	SizedValue operator ++ () { ++this->value_[0]; return *this; }
	SizedValue operator + () const { return *this; }
	InnerValueType toUint() const { return this->value_[0]; }
	InnerValueType& asUint() { return this->value_[0]; }
	bool negative() const { return this->value_.back()>>(std::numeric_limits<InnerValueType>::digits-1); }
};

// Not using long double because for e.g. x86_64 it has 128 bits.
struct EDB_EXPORT Value80 : public ValueBase<16,5> {
	Value80() = default;

	template <class Data>
	explicit Value80 (const Data& data, std::size_t offset=0) : ValueBase<16,5>(data,offset) {
		static_assert(sizeof(Value80) * 8 == 80, "Size is broken!");
	}

	template <class SmallData>
	static Value80 fromZeroExtended(const SmallData& data) {
		Value80 created;
		created.copyZeroExtended(data);
		return created;
	}

	long double toFloatValue() const {
#ifdef _MSC_VER
		double d;
		long_double_to_double(&value_, &d);
		return d;
#else
		long double float80val;
		std::memcpy(&float80val, &value_, sizeof(value_));
		return float80val;
#endif
	}

	QString toString() const {
		std::ostringstream ss;
		ss << std::showpos << std::setprecision(20) << toFloatValue();
		return QString::fromStdString(ss.str());
	}

	bool negative() const { return value_[4] & 0x8000; }
	SizedValue<16> exponent() const { return value_[4] & 0x7fff; }
	SizedValue<64> mantissa() const { return SizedValue<64>(value_); }
};

template <int N>
struct LargeSizedValue : public ValueBase<LargeSizedValueElementWidth, N / LargeSizedValueElementWidth> {
	using BaseClass = ValueBase<LargeSizedValueElementWidth, N / LargeSizedValueElementWidth>;

	static_assert(N % LargeSizedValueElementWidth == 0, "LargeSizedValue must have multiple of 64 bits in size");

	LargeSizedValue() = default;

	template <class Data>
	explicit LargeSizedValue (const Data& data, std::size_t offset = 0) : BaseClass(data,offset) {
		static_assert(sizeof(LargeSizedValue) * 8 == N, "Size is broken!");
	}

	template <class SmallData>
	static LargeSizedValue fromZeroExtended(const SmallData& data) {
		LargeSizedValue created;
		created.copyZeroExtended(data);
		return created;
	}
};

}

// GPR on x86
using value8  = detail::SizedValue<8>;
using value16 = detail::SizedValue<16>;
using value32 = detail::SizedValue<32>;

// MMX/GPR(x86_64)
using value64  = detail::SizedValue<64>;

// FPU
using value80  = detail::Value80;

// SSE
using value128 = detail::LargeSizedValue<128>;

// AVX
using value256 = detail::LargeSizedValue<256>;

// AVX512
using value512 = detail::LargeSizedValue<512>;

static_assert(std::is_standard_layout<value8>::value &&
              std::is_standard_layout<value16>::value &&
              std::is_standard_layout<value32>::value &&
              std::is_standard_layout<value64>::value &&
              std::is_standard_layout<value80>::value &&
              std::is_standard_layout<value128>::value &&
              std::is_standard_layout<value256>::value &&
              std::is_standard_layout<value512>::value, "Fixed-sized values are intended to have standard layout");

}

#endif
