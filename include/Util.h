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

#include "FloatX.h"
#include <QString>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <type_traits>
#include <boost/optional.hpp>

enum class NumberDisplayMode {
	Hex,
	Signed,
	Unsigned,
	Float
};

namespace util {

// Used to interconvert between abstract enum defined in an interface and actual enumerators in implementation
template<typename AbstractEnum, typename ConcreteEnum>
class AbstractEnumData {
	AbstractEnum data;
public:
	AbstractEnumData(AbstractEnum a) : data(a) {}
	AbstractEnumData(ConcreteEnum c) : data(static_cast<AbstractEnum>(c)) {}
	operator AbstractEnum() const { return data; }
	operator ConcreteEnum() const { return static_cast<ConcreteEnum>(data); }
};

template<typename T>
typename std::make_unsigned<T>::type to_unsigned(T x) { return x; }

//------------------------------------------------------------------------------
// Name: percentage
// Desc: calculates how much of a multi-region byte search we have completed
//------------------------------------------------------------------------------
template <class N1, class N2, class N3, class N4>
int percentage(N1 regions_finished, N2 regions_total, N3 bytes_done, N4 bytes_total) {

	// how much percent does each region account for?
	const auto region_step = 1.0 / static_cast<double>(regions_total) * 100;

	// how many regions are done?
	const double regions_complete = region_step * regions_finished;

	// how much of the current region is done?
	const double region_percent = region_step * static_cast<float>(bytes_done) / static_cast<float>(bytes_total);

	return static_cast<int>(regions_complete + region_percent);
}

//------------------------------------------------------------------------------
// Name: percentage
// Desc: calculates how much of a single-region byte search we have completed
//------------------------------------------------------------------------------
template <class N1, class N2>
int percentage(N1 bytes_done, N2 bytes_total) {
	return percentage(0, 1, bytes_done, bytes_total);
}

template<typename T> typename std::enable_if<std::is_floating_point<T>::value, QString>::type toString(T value, int precision) {
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
constexpr auto make_array(T head, Tail...tail) -> std::array<T,1+sizeof...(Tail)>
{ return std::array<T,1+sizeof...(Tail)>{head,tail...}; }

template<typename Container, typename Element>
bool contains(const Container& container, const Element& element)
{ return std::find(std::begin(container),std::end(container),element)!=std::end(container); }

template<typename Stream>
void print(Stream& str){ str << "\n"; }
template<typename Stream, typename Arg0, typename...Args>
void print(Stream& str, const Arg0& arg0, const Args&...args)
{
	str << arg0;
	print(str,args...);
}
#define EDB_PRINT_AND_DIE(...) { util::print(std::cerr,__FILE__,":",__LINE__,": ",Q_FUNC_INFO,": Fatal error: ",__VA_ARGS__); std::abort(); }

// Returns a string of `AsType`s ordered from highest in SIMD register to lowest
template<typename AsType, typename T>
typename std::enable_if<std::is_floating_point<AsType>::value, QString>::type packedFloatsToString(const T& value)
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
QString formatInt(T value, NumberDisplayMode mode)
{
	switch(mode)
	{
	case NumberDisplayMode::Hex:
		return value.toHexString();
	case NumberDisplayMode::Signed:
		return value.signedToString();
	case NumberDisplayMode::Unsigned:
		return value.unsignedToString();
	default:
		EDB_PRINT_AND_DIE("Unexpected integer display mode ",(long)mode);
	}
}

template<typename AsType, typename T>
typename std::enable_if<std::is_integral<AsType>::value, QString>::type packedIntsToString(const T& value,NumberDisplayMode mode)
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
        const int fieldWidth=(mode==NumberDisplayMode::Hex ? sizeof(AsType)*2 : decimalLength)+spacing;
		result += formatInt(v,mode).rightJustified(fieldWidth);
    }
    return result;
}

template<typename Float>
boost::optional<Float> fullStringToFloat(std::string const& s)
{
	static_assert(std::is_same<Float, float>::value ||
				  std::is_same<Float, double>::value ||
				  std::is_same<Float, long double>::value,
				  "Floating-point type not supported by this function");
	// NOTE: Don't use std::istringstream: it doesn't support hexfloat.
	//       Don't use std::sto{f,d,ld} either: they throw std::out_of_range on denormals.
	//       Only std::strto{f,d,ld} are sane, for some definitions of sane...
	const char*const str=s.c_str();
	char* end;
	Float value;
	errno=0;
	if(std::is_same<Float, float>::value)
		value=std::strtof (str, &end);
	else if(std::is_same<Float, double>::value)
		value=std::strtod (str, &end);
	else
		value=std::strtold(str, &end);
	if(errno)
	{
		if((errno==ERANGE && (value==0 || std::isinf(value))) || errno!=ERANGE)
			return boost::none;
	}

	if(end==str+s.size()) return value;

	return boost::none;
}

}

#define BIT_LENGTH(expr) (8*sizeof(expr))

#endif
