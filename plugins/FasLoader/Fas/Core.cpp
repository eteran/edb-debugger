/*
 * @file: Core.cpp
 *
 * This file part of RT ( Reconstructive Tools )
 * Copyright (c) 2018 darkprof <dark_prof@mail.ru>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 **/

#include "Core.hpp"
#include "Exception.hpp"
#include <QtDebug>

namespace {

constexpr uint32_t Signature = 0x1A736166;

}

// TODO(eteran): this class is generally a good candidate for more RAII
// and usage of the QFile class instead of std::ifstream

namespace Fas {

/**
 * @brief Load a *.fas file and process its symbols.
 *
 * @param fileName The path to the *.fas file to load.
 */
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
		qDebug() << fasSymbols_.size();
		deleteAnonymousSymbols();
		loadSymbols();
	} catch (const Exception &e) {
		qWarning() << e.what();
	}
}

/**
 * @brief Open the *.fas file for reading.
 *
 * @throws Exception if the file cannot be opened.
 */
void Core::open() {
	ifs_.open(fileName_, std::ios::binary);
	if (!ifs_.is_open()) {
		throw Exception("*.fas file not loaded.");
	}
}

/**
 * @brief Load the header of the *.fas file.
 *
 * @throws Exception if the header cannot be read or is invalid.
 */
void Core::loadHeader() {
	ifs_.seekg(0);

	if (!ifs_.read((char *)&header_, sizeof(Header))) {
		throw Exception("*.fas Header not loaded.");
	}

	if (header_.signature != Signature) {
		throw Exception("*.fas signature fail");
	}

	if (header_.lengthOfHeader != 64) {
		throw Exception("*.fas header size not supported");
	}
}

/**
 * @brief Load the symbols from the *.fas file into memory.

 * @throws Exception if any symbol cannot be read.
 */
void Core::loadFasSymbols() {
	ifs_.seekg(header_.offsetOfSymbolsTable);

	const auto size  = header_.lengthOfSymbolsTable;
	const auto count = size / sizeof(Fas::Symbol);
	for (uint i = 0; i < count; ++i) {
		auto symbol = loadFasSymbol();
		fasSymbols_.push_back(symbol);
	}
}

/**
 * @brief Load a single symbol from the *.fas file.

 * @throws Exception if the symbol cannot be read.
 */
Fas::Symbol Core::loadFasSymbol() {
	Fas::Symbol symbol;

	if (!ifs_.read((char *)&symbol, sizeof(Fas::Symbol))) {
		throw Exception("*.fas symbol not loaded");
	}

	return symbol;
}

/**
 * @brief Delete all undefined symbols from the loaded symbols.
 *
 * @throws Exception if any error occurs during deletion.
 */
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

/**
 * @brief Delete all assembly-time variable symbols from the loaded symbols.
 *
 * @throws Exception if any error occurs during deletion.
 */
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

/**
 * @brief Delete all symbols that cannot be forward referenced from the loaded symbols.
 *
 * @throws Exception if any error occurs during deletion.
 */
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

/**
 * @brief Delete all special marker symbols from the loaded symbols.
 *
 * @throws Exception if any error occurs during deletion.
 */
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

/**
 * @brief Delete all negative symbols from the loaded symbols.
 *
 * @throws Exception if any error occurs during deletion.
 */
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

/**
 * @brief Delete all anonymous symbols from the loaded symbols.
 *
 * @throws Exception if any error occurs during deletion.
 */
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

/**
 * @brief Load all symbols from the loaded *.fas file into memory.
 *
 * @throws Exception if any symbol cannot be read.
 */
void Core::loadSymbols() {
	for (auto fasSymbol : fasSymbols_) {
		checkAbsoluteValue(fasSymbol);
		loadSymbolFromFasSymbol(fasSymbol);
	}
}

/**
 * @brief Check if the symbol has an absolute value.
 *
 * @param fasSymbol The symbol to check.
 *
 * @throws Exception if the symbol does not have an absolute value.
 */
void Core::checkAbsoluteValue(const Fas::Symbol &fasSymbol) {
	if (fasSymbol.typeOfValue != ValueTypes::ABSOLUTE_VALUE) {
		throw Exception("Support only absolute value");
	}
}

/**
 * @brief Load a symbol from a Fas::Symbol into a PluginSymbol.
 *
 * @param fasSymbol The Fas::Symbol to load.
 *
 * @throws Exception if the symbol cannot be loaded.
 */
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

/**
 * @brief Convert a C-style string from the Fas::Symbol into a std::string.
 *
 * @param fasSymbol The Fas::Symbol containing the C-style string.
 *
 * @return The converted std::string.
 *
 * @throws Exception if the string cannot be read.
 */
std::string Core::cstr2string(const Symbol &fasSymbol) {

	constexpr int MaxLength = 64;

	const uint32_t offset = header_.offsetOfSymbolsTable + fasSymbol.preprocessed;
	char cstr[MaxLength];
	auto count = 0;

	ifs_.seekg(offset);
	char *c = cstr;
	while (true) {
		ifs_.read(c, 1);

		if (count >= (MaxLength - 1)) {
			break;
		}

		if (*c == '\0') {
			break;
		}

		++c;
		++count;
	}

	cstr[count] = '\0';

	std::string str(cstr, count);
	return str;
}

/**
 * @brief Convert a Pascal-style string from the Fas::Symbol into a std::string.
 *
 * @param fasSymbol The Fas::Symbol containing the Pascal-style string.
 *
 * @return The converted std::string.
 *
 * @throws Exception if the string cannot be read.
 */
std::string Core::pascal2string(const Fas::Symbol &fasSymbol) {

	constexpr int MaxLength = 256;

	const uint32_t offset = header_.offsetOfPreprocessedSource + fasSymbol.preprocessed;
	uint8_t len;

	char pascal[MaxLength];

	ifs_.seekg(offset);

	if (!ifs_.read((char *)&len, sizeof(len))) {
		throw Exception("Length of pascal string not loaded");
	}

	// NOTE(eteran): technically impossible with a MaxLength of 256, but we check anyway
	// in case we ever see reason to change the MaxLength in the future.
	if (len >= MaxLength) {
		throw Exception("Pascal string length exceeds maximum allowed length");
	}

	if (!ifs_.read(pascal, len)) {
		throw Exception("Pascal string not loaded");
	}

	std::string str(pascal, len);
	return str;
}

/**
 * @brief Get all loaded plugin symbols.
 *
 * @return A reference to the collection of loaded plugin symbols.
 */
const PluginSymbols &Core::getSymbols() {
	return symbols_;
}

}
