#pragma once

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
