/*
 * @file: Header.hpp
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

#ifndef FAS_HEADER_H_
#define FAS_HEADER_H_

#include <cstdint>

namespace Fas {

#pragma pack(push, 1)
struct Header {
	uint32_t signature;
	uint8_t major;
	uint8_t minor;
	uint16_t lengthOfHeader;
	uint32_t offsetOfInputFileName;  // base = strings table
	uint32_t offsetOfOutputFileName; // base = strings table
	uint32_t offsetOfStringsTable;
	uint32_t lengthOfStringsTable;
	uint32_t offsetOfSymbolsTable;
	uint32_t lengthOfSymbolsTable;
	uint32_t offsetOfPreprocessedSource;
	uint32_t lengthOfPreprocessedSource;
	uint32_t offsetOfAssemblyDump;
	uint32_t lengthOfAssemblyDump;
	uint32_t offsetOfSectionNamesTable;
	uint32_t lengthOfSectionNamesTable;
	uint32_t offsetOfSymbolReferencesDump;
	uint32_t lengthOfSymbolReferencesDump;
};
#pragma pack(pop)

}

#endif
