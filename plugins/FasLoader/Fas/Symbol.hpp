/*
 * @file: Symbol.hpp
 *
 * This file part of RT ( Reconstructive Tools )
 * Copyright (c) 2018 darkprof <dark_prof@mail.ru>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **/

#ifndef FAS_SYMBOL_H_
#define FAS_SYMBOL_H_

#include <cstdint>

namespace Fas {

enum class ValueTypes : uint8_t {
	ABSOLUTE_VALUE = 0,
	RELOCATABLE_SEGMENT_ADDRESS,
	RELOCATABLE_32_BIT_ADDRESS,
	RELOCATABLE_RELATIVE_32_BIT_ADDRESS,
	RELOCATABLE_64_BIT_ADDRESS,
	GOT_RELATIVE_32_BIT_ADDRESS,
	_32_BIT_ADDRESS_OF_PLT_ENTRY,
	RELATIVE_32_BIT_ADDRESS_OF_PLT_ENTRY
};

#pragma pack(push, 1)
struct Symbol {
	uint64_t value : 63;
	uint64_t valueSign : 1;

	uint16_t flags;

	uint8_t sizeOfData;
	ValueTypes typeOfValue;
	uint32_t extendedSib;
	uint16_t numberOfPassDefined;
	uint16_t numberOfPassUsed;

	uint32_t section : 31;
	uint32_t sectionSign : 1;

	uint32_t preprocessed : 31;
	uint32_t preprocessedSign : 1;

	uint32_t offsetInPreprocessed;
};
#pragma pack(pop)

}

#endif
