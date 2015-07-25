/*
Copyright (C) 2006 - 2014 Evan Teran
                          eteran@alum.rit.edu

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

#include "ArchTypes.h"
#include "OSTypes.h"
#include <cstdint>
#include <QString>
#include <type_traits>
#include <cstring>
#include <array>
#include <sstream>
#include <iomanip>
#include <cassert>
#include <cstddef>

namespace edb {

enum EVENT_STATUS {
	DEBUG_STOP,                 // do nothing, the UI will instigate the next event
	DEBUG_CONTINUE,             // the event has been addressed, continue as normal
	DEBUG_CONTINUE_STEP,        // the event has been addressed, step as normal
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
	template<typename Data>
	explicit SizedValue (const Data& data, std::size_t offset=0) : ValueBase<N,1>(data,offset)
	{ static_assert(sizeof(SizedValue)*8==N,"Size is broken!"); }

	QString toString() const { return QString("%1").arg(this->value_); }
};

// Not using long double because for e.g. x86_64 it has 128 bits.
struct Value80 : public ValueBase<16,5> {
	Value80() = default;
	template<typename Data>
	explicit Value80 (const Data& data, std::size_t offset=0) : ValueBase<16,5>(data,offset)
	{ static_assert(sizeof(Value80)*8==80,"Size is broken!"); }

	enum class FloatType {
		Zero,
		Normal,
		Infinity,
		NegativeInfinity,
		Denormal,
		PseudoDenormal,
		SNaN,
		QNaN,
		Unsupported
	};
	FloatType floatType() const {
		uint16_t exponent=this->exponent();
		uint64_t mantissa=this->mantissa();
		bool negative=value_[4] & 0x8000;
		static constexpr uint64_t integerBitOnly=0x8000000000000000ULL;
		static constexpr uint64_t QNaN_mask=0xc000000000000000ULL;
		bool integerBitSet=mantissa & integerBitOnly;
		if(exponent==0x7fff)
		{
			if(mantissa==integerBitOnly)
			{
				if(negative)
					return FloatType::NegativeInfinity; // |1|11..11|1.000..0|
				else
					return FloatType::Infinity;         // |0|11..11|1.000..0|
			}
			if((mantissa & QNaN_mask) == QNaN_mask)
				return FloatType::QNaN;                 // |X|11..11|1.1XX..X|
			else if((mantissa & QNaN_mask) == integerBitOnly)
				return FloatType::SNaN;                 // |X|11..11|1.0XX..X|
			else
				return FloatType::Unsupported; 			// integer bit reset
		}
		else if(exponent==0x0000)
		{
			if(mantissa==0)
				return FloatType::Zero;
			else
			{
				if(!integerBitSet)
					return FloatType::Denormal;     // |X|00.00|0.XXXX..X|
				else
					return FloatType::PseudoDenormal;// |X|00.00|1.XXXX..X|
			}
		}
		else
		{
			if(integerBitSet)
				return FloatType::Normal;
			else
				return FloatType::Unsupported;
		}
	}
	bool isSpecial(FloatType type) const {
		return type!=FloatType::Normal &&
			   type!=FloatType::Zero;
	}

	QString toString() const {
		std::ostringstream ss;
		long double float80val;
		std::memcpy(&float80val, &value_, sizeof value_);
		ss << std::showpos << std::setprecision(20) << float80val;
		return QString::fromStdString(ss.str());
	}

	uint16_t exponent() const { return value_[4] & 0x7fff; }
	uint64_t mantissa() const { return SizedValue<64>(value_).value()[0]; }
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
}

#endif
