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

#ifndef TYPES_20071127_H_
#define TYPES_20071127_H_

#include <QString>
#include <QVariant>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>
#include <type_traits>
#include <vector>

class Register;

namespace edb {

enum EVENT_STATUS {
	DEBUG_STOP,                 // do nothing, the UI will instigate the next event
	DEBUG_CONTINUE,             // the event has been addressed, continue as normal
	DEBUG_CONTINUE_STEP,        // the event has been addressed, step as normal
	DEBUG_CONTINUE_BP,          // the event was a BP, which we need to ignore
	DEBUG_EXCEPTION_NOT_HANDLED // pass the event unmodified back thread and continue
};

namespace detail {

template <int N>
struct sized_uint {};

template <> struct sized_uint<8>  { using type = uint8_t; };
template <> struct sized_uint<16> { using type = uint16_t; };
template <> struct sized_uint<32> { using type = uint32_t; };
template <> struct sized_uint<64> { using type = uint64_t; };

template <int ElementWidth, int ElementCount>
class ValueBase {
	static_assert(ElementCount > 0, "ValueBase::value_ must be non-empty");

protected:
	using ValueType = std::array<typename sized_uint<ElementWidth>::type, ElementCount>;
	ValueType value_;

	explicit ValueBase(const std::vector<std::uint8_t>& data,std::size_t offset = 0) {
		assert(data.size() - offset >= sizeof(ValueType)); // check bounds, this can't be done at compile time

		auto dataStart = reinterpret_cast<const char*>(&data);
		std::memcpy(&value_, dataStart + offset, sizeof(value_));
	}

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
		std::memset(target + sizeof data, 0, sizeof(value_) - sizeof(data));
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
	explicit SizedValue (const Data& data, std::size_t offset = 0) : ValueBase<N,1>(data,offset) {
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
	operator QVariant() const { return QVariant::fromValue(this->value_[0]); }

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
struct Value80 : public ValueBase<16,5> {
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
		long double float80val;
		std::memcpy(&float80val, &value_, sizeof(value_));
		return float80val;
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

static constexpr int LargeSizedValueElementWidth = 64;

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

class address_t : public value64 {
public:
	QString toPointerString(bool createdFromNativePointer = true) const;
	QString toHexString() const;

	template <class SmallData>
	static address_t fromZeroExtended(const SmallData& data) {
		return value64::fromZeroExtended(data);
	}

	template<class T>
	address_t(const T& val) : value64(val) {
	}

	address_t() = default;
	void normalize();
};

using reg_t = address_t;

}

template<class T> typename std::enable_if<std::is_same<T,edb::value8 >::value ||
										  std::is_same<T,edb::value16>::value ||
										  std::is_same<T,edb::value32>::value ||
										  std::is_same<T,edb::value64>::value ||
										  std::is_same<T,edb::reg_t>::value   ||
										  std::is_same<T,edb::address_t>::value,
std::istream&>::type operator>>(std::istream& os, T& val)
{
	os >> val.asUint();
	return os;
}

template<class T> typename std::enable_if<std::is_same<T,edb::value8 >::value ||
										  std::is_same<T,edb::value16>::value ||
										  std::is_same<T,edb::value32>::value ||
										  std::is_same<T,edb::value64>::value ||
										  std::is_same<T,edb::reg_t>::value   ||
										  std::is_same<T,edb::address_t>::value,
std::ostream&>::type operator<<(std::ostream& os, T val)
{
	os << val.toUint();
	return os;
}

/* Comment Type */
struct Comment {
	edb::address_t 	address;
	QString        	comment;
};

#include "ArchTypes.h"
#endif
