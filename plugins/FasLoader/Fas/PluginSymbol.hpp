/*
 * @file: PluginSymbol.cpp
 *
 * This file part of RT ( Reconstructive Tools )
 * Copyright (c) 2018 darkprof <dark_prof@mail.ru>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 **/

#ifndef FAS_PLUGIN_SYMBOL_H_
#define FAS_PLUGIN_SYMBOL_H_

#include <cstdint>
#include <string>

namespace Fas {

struct PluginSymbol {
	uint64_t value;
	std::string name;
	uint8_t size;
};

}

#endif
