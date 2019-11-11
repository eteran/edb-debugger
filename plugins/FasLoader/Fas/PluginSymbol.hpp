#pragma once

#include <cstdint>
#include <string>

namespace Fas {

struct PluginSymbol {
	uint64_t value;
	std::string name;
	uint8_t size;
};

}
