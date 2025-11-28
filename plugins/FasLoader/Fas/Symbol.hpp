/*
 * @file: Symbol.hpp
 *
 * This file part of RT ( Reconstructive Tools )
 * Copyright (c) 2018 darkprof <dark_prof@mail.ru>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
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
