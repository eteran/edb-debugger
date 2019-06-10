
#include "Core.hpp"
#include "Exception.hpp"
#include <QtDebug>

namespace {

constexpr uint32_t Signature = 0x1A736166;

}

// TODO(eteran): this class is generally a good candiate for more RAII
// and usage of the QFile class instead of std::ifstream

namespace Fas {

void Core::load(const std::string &fileName) {
	fileName_ = fileName;


	try {
		open();
		loadHeader();
		loadFasSymbols();
		deleteUndefinedSymbols();
		deleteAssemblyTimeVariable();
		deleteCannotBeForwardReferenced();
		deleteNegativeSymbols();
		deleteSpecialMarkers();
		qInfo() << fasSymbols_.size();
		deleteAnonymousSymbols();
		loadSymbols();
	} catch (std::exception &e) {
		qWarning() << e.what();
	}
}

void Core::open() {
	ifs_.open(fileName_, std::ios::binary);
	if (!ifs_.is_open()) {
		throw Exception("*.fas file not loaded.");
	}
}

void Core::loadHeader() {
	ifs_.seekg(0);
	ifs_.read((char *)&header_, sizeof(Header));

	if (!ifs_.good()) {
		throw Exception("*.fas Header not loaded.");
	}

	if (header_.signature != Signature) {
		throw Exception("*.fas signature fail");
	}

	if (header_.lengthOfHeader != 64) {
		throw Exception("*.fas header size not supported");
	}
}

void Core::loadFasSymbols() {
	ifs_.seekg(header_.offsetOfSymbolsTable);

	auto size  = header_.lengthOfSymbolsTable;
	auto count = size / sizeof(Fas::Symbol);
	for (uint i = 0; i < count; ++i) {
		auto symbol = loadFasSymbol();
		fasSymbols_.push_back(symbol);
	}
}

Fas::Symbol Core::loadFasSymbol() {
	Fas::Symbol symbol;
	ifs_.read((char *)&symbol, sizeof(Fas::Symbol));

	if (!ifs_.good()) {
		throw Exception("*.fas symbol not loaded");
	}

	return symbol;
}

void Core::deleteUndefinedSymbols() {
	auto it = std::begin(fasSymbols_);
	while (it != std::end(fasSymbols_)) {
		uint16_t wasDefined = it->flags & 1;
		if (!wasDefined) {
			it = fasSymbols_.erase(it);
		} else {
			++it;
		}
	}
}

void Core::deleteAssemblyTimeVariable() {
	auto it = std::begin(fasSymbols_);
	while (it != std::end(fasSymbols_)) {
		uint16_t isAssemblyTimeVariable = it->flags & 0x2;
		if (isAssemblyTimeVariable) {
			it = fasSymbols_.erase(it);
		} else {
			++it;
		}
	}
}

void Core::deleteCannotBeForwardReferenced() {
	auto it = std::begin(fasSymbols_);
	while (it != std::end(fasSymbols_)) {
		uint16_t cannotBeForwardReferenced = it->flags & 0x4;
		if (cannotBeForwardReferenced) {
			it = fasSymbols_.erase(it);
		} else {
			++it;
		}
	}
}

void Core::deleteSpecialMarkers() {
	auto it = std::begin(fasSymbols_);
	while (it != std::end(fasSymbols_)) {
		uint16_t isSpecialMarker = it->flags & 0x400;
		if (isSpecialMarker) {
			it = fasSymbols_.erase(it);
		} else {
			++it;
		}
	}
}

void Core::deleteNegativeSymbols() {
	auto it = std::begin(fasSymbols_);
	while (it != std::end(fasSymbols_)) {
		if (it->valueSign) {
			it = fasSymbols_.erase(it);
		} else {
			++it;
		}
	}
}

void Core::deleteAnonymousSymbols() {
	auto it = std::begin(fasSymbols_);
	while (it != std::end(fasSymbols_)) {
		bool isAnonymous = it->preprocessedSign == 0 && it->preprocessed == 0;
		if (isAnonymous) {
			it = fasSymbols_.erase(it);
		} else {
			++it;
		}
	}
}

void Core::loadSymbols() {
	for (auto fasSymbol : fasSymbols_) {
		checkAbsoluteValue(fasSymbol);
		loadSymbolFromFasSymbol(fasSymbol);
	}
}

void Core::checkAbsoluteValue(const Fas::Symbol &fasSymbol) {
	if (fasSymbol.typeOfValue != ValueTypes::ABSOLUTE_VALUE) {
		throw Exception(" Support only absolute value");
	}
}

void Core::loadSymbolFromFasSymbol(const Fas::Symbol &fasSymbol) {
	PluginSymbol symbol;

	symbol.value = fasSymbol.value;
	symbol.size  = fasSymbol.sizeOfData;

	if (fasSymbol.preprocessedSign) {
		// in the strings table
		symbol.name = cstr2string(fasSymbol);
	} else {
		// in the preprocessed pascal style
		symbol.name = pascal2string(fasSymbol);
	}

	symbols_.push_back(symbol);
}

std::string Core::cstr2string(const Symbol &fasSymbol) {
	std::string str;
	auto        offset  = header_.offsetOfSymbolsTable + fasSymbol.preprocessed;
	const int   MAX_LEN = 64;
	char        cstr[MAX_LEN];
	auto        count = 0;

	ifs_.seekg(offset);
	char *c = (char *)&cstr;
	while (true) {
		ifs_.read(c, 1);
		if (count >= (MAX_LEN - 1)) break;
		if (*c == 0) break;
		++c;
		++count;
	}

	cstr[count] = '\0';
	str         = cstr;

	return str;
}

std::string Core::pascal2string(const Fas::Symbol &fasSymbol) {

	std::string str;
	auto        offset = header_.offsetOfPreprocessedSource + fasSymbol.preprocessed;
	uint8_t     len;
	char        pascal[64];

	ifs_.seekg(offset);
	ifs_.read((char *)&len, sizeof(len));
	if (!ifs_.good()) {
		throw Exception("Length of pascal string not loaded");
	}

	ifs_.read((char *)&pascal, len);
	if (!ifs_.good()) {
		throw Exception("Pascal string not loaded");
	}
	pascal[len] = '\0';

	str = pascal;

	return str;
}

const PluginSymbols &Core::getSymbols() {
	return symbols_;
}

}
