#pragma once

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
