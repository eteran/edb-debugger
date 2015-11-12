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

#ifndef UTIL_20061126_H_
#define UTIL_20061126_H_

#include <QString>
#include <sstream>
#include <iomanip>
#include <type_traits>
#include <cstddef>
#include <array>
#include <algorithm>
#include "FloatX.h"

enum class IntDisplayMode {
	Hex,
	Signed,
	Unsigned
};

namespace util {

//------------------------------------------------------------------------------
// Name: percentage
// Desc: calculates how much of a multi-region byte search we have completed
//------------------------------------------------------------------------------
inline int percentage(int regions_finished, int regions_total, int bytes_done, int bytes_total) {

	// how much percent does each region account for?
	const auto region_step      = 1.0 / static_cast<double>(regions_total) * 100;

	// how many regions are done?
	const double regions_complete = region_step * regions_finished;

	// how much of the current region is done?
	const double region_percent   = region_step * static_cast<float>(bytes_done) / static_cast<float>(bytes_total);

	return static_cast<int>(regions_complete + region_percent);
}

//------------------------------------------------------------------------------
// Name: percentage
// Desc: calculates how much of a single-region byte search we have completed
//------------------------------------------------------------------------------
inline int percentage(int bytes_done, int bytes_total) {
	return percentage(0, 1, bytes_done, bytes_total);
}

template<typename T> typename std::enable_if<std::is_floating_point<T>::value,
QString>::type toString(T value, int precision) {
	std::ostringstream ss;
	ss << std::setprecision(precision) << value;
	return QString::fromStdString(ss.str());
}

inline void markMemory(void* memory, std::size_t size)
{
	char* p=reinterpret_cast<char*>(memory);
	// Fill memory with 0xbad1bad1 marker
	for(std::size_t i=0;i<size;++i)
		p[i]=i&1 ? 0xba : 0xd1;
}

template<typename T, typename... Tail>
auto make_array(T head, Tail...tail) -> std::array<T,1+sizeof...(Tail)>
{ return std::array<T,1+sizeof...(Tail)>{head,tail...}; }

template<typename Container, typename Element>
bool contains(const Container& container, const Element& element)
{ return std::find(std::begin(container),std::end(container),element)!=std::end(container); }

// Returns a string of `AsType`s ordered from highest in SIMD register to lowest
template<typename AsType, typename T>
typename std::enable_if<std::is_floating_point<AsType>::value,
QString>::type packedFloatsToString(const T& value)
{
    auto p=reinterpret_cast<const char*>(&value);
    const std::size_t elementCount=sizeof value/sizeof(AsType);
    QString result;
    for(std::size_t i=elementCount-1;i!=std::size_t(-1);--i)
    {
		edb::detail::SizedValue<sizeof(AsType)*8> v;
        std::memcpy(&v,&p[i*sizeof(AsType)],sizeof(AsType));

		static const int spacing=1;
        static const int fieldWidth=maxPrintedLength<AsType>()+spacing;
		result += formatFloat(v).rightJustified(fieldWidth);
    }
    return result;
}

template<typename T>
QString formatInt(T value, IntDisplayMode mode)
{
	switch(mode)
	{
	case IntDisplayMode::Hex:
		return value.toHexString();
	case IntDisplayMode::Signed:
		return value.signedToString();
	case IntDisplayMode::Unsigned:
		return value.unsignedToString();
	default:
		Q_ASSERT(!"Unexpected integer display mode");
		return "???";
	}
}

template<typename AsType, typename T>
typename std::enable_if<std::is_integral<AsType>::value,
QString>::type packedIntsToString(const T& value,IntDisplayMode mode)
{
    auto p=reinterpret_cast<const char*>(&value);
    const std::size_t elementCount=sizeof value/sizeof(AsType);
    QString result;
    for(std::size_t i=elementCount-1;i!=std::size_t(-1);--i)
    {
		edb::detail::SizedValue<sizeof(AsType)*8> v;
        std::memcpy(&v,&p[i*sizeof(AsType)],sizeof(AsType));

		static const int spacing=1;
		static const int decimalLength=maxPrintedLength<AsType>();
        const int fieldWidth=(mode==IntDisplayMode::Hex ? sizeof(AsType)*2 : decimalLength)+spacing;
		result += formatInt(v,mode).rightJustified(fieldWidth);
    }
    return result;
}

}

#define BIT_LENGTH(expr) (8*sizeof(expr))

#endif
