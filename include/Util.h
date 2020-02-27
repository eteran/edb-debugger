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

#ifndef UTIL_H_20061126_
#define UTIL_H_20061126_

namespace util {

// Used to interconvert between abstract enum defined in an interface and actual enumerators in implementation
template <typename AbstractEnum, typename ConcreteEnum>
class AbstractEnumData {
	AbstractEnum data;

public:
	AbstractEnumData(AbstractEnum a)
		: data(a) {
	}

	AbstractEnumData(ConcreteEnum c)
		: data(static_cast<AbstractEnum>(c)) {
	}

	operator AbstractEnum() const { return data; }
	operator ConcreteEnum() const { return static_cast<ConcreteEnum>(data); }
};

inline void mark_memory(void *memory, std::size_t size) {

	auto p = reinterpret_cast<uint8_t *>(memory);

	// Fill memory with 0xbad1bad1 marker
	for (std::size_t i = 0; i < size; ++i) {
		p[i] = (i & 1) ? 0xba : 0xd1;
	}
}

}

#endif
