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

#include "OSTypes.h"
#include <cstdint>
#include <QString>
#include <QVariant>
#include <type_traits>
#include <cstring>
#include <array>
#include <sstream>
#include <iomanip>
#include <cassert>
#include <cstddef>
#include <limits>
#include <map>

class Register;

namespace edb {

enum EVENT_STATUS {
	DEBUG_STOP,                 // do nothing, the UI will instigate the next event
	DEBUG_CONTINUE,             // the event has been addressed, continue as normal
	DEBUG_CONTINUE_STEP,        // the event has been addressed, step as normal
	DEBUG_CONTINUE_BP,          // the event was a BP, which we need to ignore
	DEBUG_EXCEPTION_NOT_HANDLED // pass the event unmodified back thread and continue
};

namespace detail
{
template<int N>
struct sized_uint {};
template<> struct sized_uint<8> { typedef uint8_t type; };
template<> struct sized_uint<16> { typedef uint16_t type; };
template<> struct sized_uint<32> { typedef uint32_t type; };
template<> struct sized_uint<64> { typedef uint64_t type; };

template<int ELEMENT_WIDTH, int ELEMENT_COUNT>
class ValueBase
{
	static_assert(ELEMENT_COUNT>0,"ValueBase::value_ must be non-empty");
protected:
	typedef std::array<typename sized_uint<ELEMENT_WIDTH>::type,ELEMENT_COUNT> ValueType;
	ValueType value_;

	template<typename Data>
	explicit ValueBase(const Data& data, std::size_t offset=0) {
		static_assert(sizeof(Data)>=sizeof(ValueType),"ValueBase can only be constructed from large enough variable");
		assert(sizeof(Data)-offset>=sizeof(ValueType)); // check bounds, this can't be done at compile time
		const char* const dataStart = reinterpret_cast<const char*>(&data);
		std::memcpy(&value_, dataStart+offset, sizeof value_);
	}
	template<typename SmallData>
	void copyZeroExtended(const SmallData& data) {
		static_assert(sizeof(SmallData)<=sizeof(ValueType),"It doesn't make sense to expand a larger type into a smaller type");
		const char* const dataStart = reinterpret_cast<const char*>(&data);
		char* const target = reinterpret_cast<char*>(&value_);
		std::memcpy(target, dataStart, sizeof data);
		std::memset(target+sizeof data, 0, sizeof(value_)-sizeof(data));
	}
	ValueBase() = default;
public:
	QString toHexString() const {
		std::ostringstream ss;
		for(auto it = value_.rbegin(); it!=value_.rend(); ++it)
			ss << std::setw(sizeof(*it)*2) << std::setfill('0') << std::hex << +*it; // + to prevent printing uint8_t as a character
		return QString::fromStdString(ss.str());
	}

	ValueType& value() { return value_; }
	const ValueType& value() const { return value_; }
	bool operator==(const ValueBase& other) const { return value_==other.value_; }
	bool operator!=(const ValueBase& other) const { return !(*this==other); }
};

template<int N>
struct SizedValue : public ValueBase<N,1> {
	typedef typename sized_uint<N>::type InnerValueType;
	static_assert(N%8==0,"SizedValue must have multiple of 8 bits in size");
	SizedValue() = default;
	template<typename Data, typename = typename std::enable_if<!std::is_floating_point<Data>::value && !std::is_integral<Data>::value>::type>
	explicit SizedValue (const Data& data, std::size_t offset=0) : ValueBase<N,1>(data,offset)
	{ static_assert(sizeof(SizedValue)*8==N,"Size is broken!"); }
	template<typename Float, typename dummy=void, typename check=typename std::enable_if<std::is_floating_point<Float>::value>::type>
	explicit SizedValue(Float floatVal) { this->value_[0]=floatVal; }
	template<typename Integer, typename = typename std::enable_if<std::is_integral<Integer>::value>::type>
	SizedValue(Integer integer) { this->value_[0]=integer; }
	SizedValue(const Register&)=delete;

	template<typename SmallData>
	static SizedValue fromZeroExtended(const SmallData& data) {
		SizedValue created;
		created.copyZeroExtended(data);
		return created;
	}

	static SizedValue fromString(const QString& str, bool* ok=nullptr, int base=10, bool Signed=false) {
		qulonglong v;
		if(Signed)
			v=str.toLongLong(ok, base);
		else
			v=str.toULongLong(ok, base);
		if(!*ok)
			return SizedValue(0);
		// Check that the result fits into InnerValueType
		SizedValue result(v);
		if(result==v) return result;
		if(ok!=nullptr) *ok=false;
		return SizedValue(0);
	}
	static SizedValue fromHexString(const QString& str, bool* ok=nullptr) { return fromString(str,ok,16); }
	static SizedValue fromSignedString(const QString& str, bool* ok=nullptr) { return fromString(str,ok,10,true); }
	static SizedValue fromCString(const QString& str, bool* ok=nullptr) { return fromString(str,ok,0); }

	operator InnerValueType() const { return this->value_[0]; }
	operator QVariant() const { return QVariant::fromValue(this->value_[0]); }

	SizedValue signExtended(std::size_t valueLength) const {
		SizedValue result(*this);
		if(valueLength==sizeof(*this)) return result;
		if(this->value_[0]&(1ull << (valueLength*8-1))) {
			result=-1ll;
			std::memcpy(&result,this,valueLength);
		}
		return result;
	}

	QString toString() const { return QString("%1").arg(this->value_[0]); }
	QString unsignedToString() const { return toString(); }
	QString signedToString() const { return QString("%1").arg(typename std::make_signed<InnerValueType>::type(this->value_[0])); }
	using ValueBase<N,1>::operator==;
	using ValueBase<N,1>::operator!=;
	template<typename RHS> typename std::enable_if<std::is_integral<RHS>::value, bool>::type operator == (RHS rhs) const { return this->value_[0] == static_cast<InnerValueType>(rhs); }
	template<typename RHS> typename std::enable_if<std::is_integral<RHS>::value, bool>::type operator != (RHS rhs) const { return this->value_[0] != static_cast<InnerValueType>(rhs); }
	template<typename RHS> typename std::enable_if<std::is_integral<RHS>::value, bool>::type operator >  (RHS rhs) const { return this->value_[0] >  rhs; }
	template<typename RHS> typename std::enable_if<std::is_integral<RHS>::value, bool>::type operator <  (RHS rhs) const { return this->value_[0] <  rhs; }
	template<typename RHS> typename std::enable_if<std::is_integral<RHS>::value, bool>::type operator >= (RHS rhs) const { return this->value_[0] >= rhs; }
	template<typename RHS> typename std::enable_if<std::is_integral<RHS>::value, bool>::type operator <= (RHS rhs) const { return this->value_[0] <= rhs; }
	template<typename RHS> typename std::enable_if<std::is_integral<RHS>::value, SizedValue>::type operator + (RHS rhs) const { return SizedValue(this->value_[0] + rhs); }
	template<typename RHS> typename std::enable_if<std::is_integral<RHS>::value, SizedValue>::type operator - (RHS rhs) const { return SizedValue(this->value_[0] - rhs); }
	template<typename RHS> typename std::enable_if<std::is_integral<RHS>::value, SizedValue>::type operator & (RHS rhs) const { return SizedValue(this->value_[0] & rhs); }
	template<typename RHS> typename std::enable_if<std::is_integral<RHS>::value, SizedValue>::type operator % (RHS rhs) const { return SizedValue(this->value_[0] % rhs); }
	template<typename RHS> typename std::enable_if<std::is_integral<RHS>::value, SizedValue>::type operator += (RHS rhs) { this->value_[0] += rhs; return *this; }
	template<typename RHS> typename std::enable_if<std::is_integral<RHS>::value, SizedValue>::type operator -= (RHS rhs) { this->value_[0] -= rhs; return *this; }
	template<typename RHS> typename std::enable_if<std::is_integral<RHS>::value, SizedValue>::type operator ^= (RHS rhs) { this->value_[0] ^= rhs; return *this; }
	template<typename RHS> typename std::enable_if<std::is_integral<RHS>::value, SizedValue>::type operator &= (RHS rhs) { this->value_[0] &= rhs; return *this; }
	template<typename RHS> typename std::enable_if<std::is_integral<RHS>::value, SizedValue>::type operator |= (RHS rhs) { this->value_[0] |= rhs; return *this; }
	template<typename RHS> typename std::enable_if<std::is_integral<RHS>::value, SizedValue>::type operator >> (RHS rhs) const { return SizedValue(this->value_[0] >> rhs); }
	template<typename RHS> typename std::enable_if<std::is_integral<RHS>::value, SizedValue>::type operator << (RHS rhs) const { return SizedValue(this->value_[0] << rhs); }
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
	template<typename Data>
	explicit Value80 (const Data& data, std::size_t offset=0) : ValueBase<16,5>(data,offset)
	{ static_assert(sizeof(Value80)*8==80,"Size is broken!"); }

	template<typename SmallData>
	static Value80 fromZeroExtended(const SmallData& data) {
		Value80 created;
		created.copyZeroExtended(data);
		return created;
	}

	long double toFloatValue() const {
		long double float80val;
		std::memcpy(&float80val, &value_, sizeof value_);
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

static constexpr int LARGE_SIZED_VALUE_ELEMENT_WIDTH=64;
template<int N>
struct LargeSizedValue : public ValueBase<LARGE_SIZED_VALUE_ELEMENT_WIDTH,N/LARGE_SIZED_VALUE_ELEMENT_WIDTH> {
	typedef ValueBase<LARGE_SIZED_VALUE_ELEMENT_WIDTH,N/LARGE_SIZED_VALUE_ELEMENT_WIDTH> BaseClass;
	static_assert(N % LARGE_SIZED_VALUE_ELEMENT_WIDTH==0,"LargeSizedValue must have multiple of 64 bits in size");
	LargeSizedValue() = default;
	template<typename Data>
	explicit LargeSizedValue (const Data& data, std::size_t offset=0) : BaseClass(data,offset)
	{ static_assert(sizeof(LargeSizedValue)*8==N,"Size is broken!"); }

	template<typename SmallData>
	static LargeSizedValue fromZeroExtended(const SmallData& data) {
		LargeSizedValue created;
		created.copyZeroExtended(data);
		return created;
	}
};
}

// GPR on x86
typedef detail::SizedValue<8> value8;
typedef detail::SizedValue<16> value16;
typedef detail::SizedValue<32> value32;
// MMX/GPR(x86_64)
typedef detail::SizedValue<64> value64;
// FPU
typedef detail::Value80 value80;
// SSE
typedef detail::LargeSizedValue<128> value128;
// AVX
typedef detail::LargeSizedValue<256> value256;
// AVX512
typedef detail::LargeSizedValue<512> value512;

static_assert(std::is_standard_layout<value8>::value &&
			  std::is_standard_layout<value16>::value &&
			  std::is_standard_layout<value32>::value &&
			  std::is_standard_layout<value64>::value &&
			  std::is_standard_layout<value80>::value &&
			  std::is_standard_layout<value128>::value &&
			  std::is_standard_layout<value256>::value &&
			  std::is_standard_layout<value512>::value,"Fixed-sized values are intended to have standard layout");

struct address_t : public value64 {
	QString toPointerString(bool createdFromNativePointer=true) const;
	QString toHexString() const;
	template<typename SmallData>
	static address_t fromZeroExtended(const SmallData& data) {
		return value64::fromZeroExtended(data);
	}
	template<class T>
	address_t(const T& val) : value64(val) {}
	address_t()=default;
	void normalize();
};

	typedef address_t                                  reg_t;
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

#include "ArchTypes.h"
#endif
