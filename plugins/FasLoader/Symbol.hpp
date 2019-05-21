#pragma once

enum class ValueTypes : uint8_t
{
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
struct Symbol
{
  uint64_t value;

  uint16_t wasDefined : 1;
  uint16_t assemblyTime : 1;
  uint16_t cannotBeForward_referenced : 1;
  uint16_t wasUsed : 1;
  uint16_t predictionWasNeededSymbolWasUsed : 1;
  uint16_t predictedResultBeingUsed : 1;
  uint16_t predictionWasNeededSymbolWasDefined : 1;
  uint16_t predictedResultBeingDefined : 1;
  uint16_t optimizationAdjustment : 1;
  uint16_t negativeEncodedAsTwosComplement : 1;
  uint16_t specialMarker : 1;
  uint16_t reserved : 5;

  uint8_t sizeOfData;
  ValueTypes typeOfValue;
  uint32_t extendedSIB;
  uint16_t numberOfPassDefined;
  uint16_t numberOfPassUsed;

  uint32_t section : 31;
  uint32_t highestSection : 1;

  uint32_t preprocessed : 31;
  uint32_t highestPreprocessed : 1;

  uint32_t offsetInPreprocessed;
};
#pragma pack(pop)
