#pragma once

#include <stdint.h>
#include <string>

struct PluginSymbol
{
  uint64_t value;
  std::string name;
  uint8_t size;
};

