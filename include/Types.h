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

template<int N>
class SizedValue {
	typedef typename sized_uint<N>::type ValueType;
	typename std::enable_if<N%8==0 && N<=64,ValueType>::type value_;
public:
	SizedValue() = default;
	explicit SizedValue(ValueType value) :value_(value) {}

	template<typename Data>
	explicit SizedValue (const Data& data, std::size_t offset=0) {
		static_assert(sizeof(Data)>=sizeof(ValueType),"SizedValue can only be constructed from large enough variable");
		assert(sizeof(Data)-offset>=sizeof(ValueType)); // check bounds, this can't be done at compile time
		const char* const dataStart = reinterpret_cast<const char*>(&data);
		std::memcpy(&value_, dataStart+offset, sizeof value_);
	}

	ValueType& value() { return value_; }
	const ValueType& value() const { return value_; }

	bool operator==(const SizedValue& other) const { return value_==other.value_; }
	bool operator!=(const SizedValue& other) const { return !(*this==other); }

	QString toHexString() const { return QString("%1").arg(value_, sizeof(value_)*2, 16, QChar('0')); }
	QString toString() const { return QString("%1").arg(value_); }
};

template<typename T>
QString makeHexString(const T& value) {
    std::ostringstream ss;
    for(auto it = value.rbegin(); it!=value.rend(); ++it)
        ss << std::setw(sizeof(*it)*2) << std::setfill('0') << std::hex << *it;
    return QString::fromStdString(ss.str());
}

class Value80 {
    // Not using long double because for e.g. x86_64 it has 128 bits.
    // If we did, it would in particular break current comparison operators
    // And anyway this class is Value80, not ValueAtLeast80.
	typedef std::array<uint16_t,5> ValueType;
	ValueType value_;
public:
	Value80() = default;
	explicit Value80(ValueType value) :value_(value) {}

	template<typename Data>
	explicit Value80 (const Data& data, std::size_t offset=0) {
		static_assert(sizeof(Data)>=sizeof(ValueType),"Value80 can only be constructed from large enough variable");
		assert(sizeof(Data)-offset>=sizeof(ValueType)); // check bounds, this can't be done at compile time
		const char* const dataStart = reinterpret_cast<const char*>(&data);
		std::memcpy(&value_, dataStart+offset, sizeof value_);
	}

	ValueType& value() { return value_; }
	const ValueType& value() const { return value_; }

	bool operator==(const Value80& other) const {
		// Can't directly compare because:
		// * if it's nan, it's unequal to anything, this would mislead the user if highlighted
		// * nans have non-unique representation and type, and we want to see _all_ the differences
		// * if it's zero, it may have different sign but be equal, we want to see sign change
		// Thus just compare per-byte
		return !std::memcmp(&value_,&other.value_, sizeof value_);
	}
	bool operator!=(const Value80& other) const { return !(*this==other); }
	QString toHexString() const { return makeHexString(value_); }

	QString toString() const {
		std::ostringstream ss;
        long double float80val;
        std::memcpy(&float80val, &value_, sizeof value_);
		ss << std::showpos << std::setprecision(16) << float80val;
		return QString::fromStdString(ss.str());
	}
};

template<int N>
class LargeSizedValue {
	typedef std::array<uint64_t,N/64> ValueType;
	typename std::enable_if<N%8==0 && (N>64),ValueType>::type value_;
public:
	LargeSizedValue() = default;
	explicit LargeSizedValue(ValueType value) :value_(value) {}

	template<typename Data>
	explicit LargeSizedValue (const Data& data, std::size_t offset=0) {
		static_assert(sizeof(Data)>=sizeof(ValueType),"LargeSizedValue can only be constructed from large enough variable");
		assert(sizeof(Data)-offset>=sizeof(ValueType)); // check bounds, this can't be done at compile time
		const char* const dataStart = reinterpret_cast<const char*>(&data);
		std::memcpy(&value_, dataStart+offset, sizeof value_);
	}

	ValueType& value() { return value_; }
	const ValueType& value() const { return value_; }

	bool operator==(const LargeSizedValue& other) const { return value_==other.value_; }
	bool operator!=(const LargeSizedValue& other) const { return !(*this==other); }
	QString toHexString() const { return makeHexString(value_); }
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
	
}

#endif
