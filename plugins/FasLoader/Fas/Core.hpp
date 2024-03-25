/*
 * @file: Core.hpp
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

#ifndef FAS_CORE_H_
#define FAS_CORE_H_

#include "Header.hpp"
#include "PluginSymbol.hpp"
#include "Symbol.hpp"

#include <fstream>
#include <string>
#include <vector>

namespace Fas {

using FasSymbols    = std::vector<Symbol>;
using PluginSymbols = std::vector<PluginSymbol>;

class Core {
public:
	Core() = default;

public:
	void load(const std::string &fileName_);
	const PluginSymbols &getSymbols();

private:
	void open();
	void loadHeader();
	void loadFasSymbols();
	Symbol loadFasSymbol();
	void deleteUndefinedSymbols();
	void deleteCannotBeForwardReferenced();
	void deleteAssemblyTimeVariable();
	void deleteSpecialMarkers();
	void deleteNegativeSymbols();
	void deleteAnonymousSymbols();
	void loadSymbols();
	void checkAbsoluteValue(const Symbol &fasSymbol);
	void loadSymbolFromFasSymbol(const Symbol &fasSymbol);
	std::string pascal2string(const Symbol &fasSymbol);
	std::string cstr2string(const Symbol &fasSymbol);

private:
	std::ifstream ifs_;
	std::string fileName_;
	Header header_;
	FasSymbols fasSymbols_;
	PluginSymbols symbols_;
};

}

#endif
