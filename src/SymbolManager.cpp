/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "SymbolManager.h"
#include "Configuration.h"
#include "ISymbolGenerator.h"
#include "Symbol.h"
#include "edb.h"

#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QProcess>
#include <QtDebug>

#include <fstream>
#include <iostream>
#include <istream>

/**
 * @brief Clears all symbols and labels from the symbol manager.
 */
void SymbolManager::clear() {
	symbolFiles_.clear();
	symbols_.clear();
	symbolsByAddress_.clear();
	symbolsByFile_.clear();
	symbolsByName_.clear();
	labels_.clear();
	labelsByName_.clear();
}

/**
 * @brief Loads a symbol file and processes its contents.
 *
 * @param filename The path to the symbol file.
 * @param base The base address for the symbols.
 */
void SymbolManager::loadSymbolFile(const QString &filename, edb::address_t base) {

	const QString symbol_directory = edb::v1::config().symbol_path;

	if (symbol_directory.isEmpty()) {
		if (showPathNotice_) {
			qDebug() << "No symbol path specified. Please set it in the preferences to enable symbols.";
			showPathNotice_ = false;
		}
		return;
	}

	// ensure that the directory exists
	QDir().mkpath(symbol_directory);

	QFileInfo info(filename);

	if (info.exists() && info.isReadable()) {

		if (info.isRelative()) {
			info.makeAbsolute();
		}

		const auto path    = QStringLiteral("%1/%2").arg(symbol_directory, info.absolutePath());
		const QString name = info.fileName();

		// ensure that the sub-directory exists
		QDir().mkpath(path);

		if (!symbolFiles_.contains(info.absoluteFilePath())) {
			const auto map_file = QStringLiteral("%1/%2.map").arg(path, name);

			if (processSymbolFile(map_file, base, filename, true)) {
				symbolFiles_.insert(info.absoluteFilePath());
			}
		}
	}
}

/**
 * @brief Finds a symbol by its name.
 *
 * @param name The name of the symbol to find.
 * @return An optional containing the symbol if found, or std::nullopt if not found.
 */
std::optional<Symbol> SymbolManager::find(const QString &name) const {

	auto it = symbolsByName_.find(name);
	if (it != symbolsByName_.end()) {
		return it.value();
	}

	// slow path... look for any symbol which matches the name, but skipping the prefix
	// we can make this faster later at the cost of yet another hash table if we
	// feel the need
	auto it2 = std::find_if(symbols_.begin(), symbols_.end(), [&name](const Symbol &symbol) {
		return symbol.name_no_prefix == name;
	});

	if (it2 != symbols_.end()) {
		return *it2;
	}

	return std::nullopt;
}

/**
 * @brief Finds all symbols that match a given name.
 *
 * @param name The name of the symbols to find.
 * @return A vector containing all matching symbols.
 */
std::vector<Symbol> SymbolManager::findAll(const QString &name) const {
	std::vector<Symbol> result;

	auto it2 = std::find_if(symbols_.begin(), symbols_.end(), [&name](const Symbol &symbol) {
		return symbol.name_no_prefix == name;
	});

	if (it2 != symbols_.end()) {
		result.push_back(*it2);
	}

	return result;
}

/**
 * @brief Finds a symbol by its address.
 *
 * @param address The address of the symbol to find.
 * @return An optional containing the symbol if found, or std::nullopt if not found.
 */
std::optional<Symbol> SymbolManager::find(edb::address_t address) const {
	auto it = symbolsByAddress_.find(address);
	if (it != symbolsByAddress_.end()) {
		return it.value();
	}

	return std::nullopt;
}

/**
 * @brief Finds a symbol near a given address.
 *
 * @param address The address to search for a nearby symbol.
 * @return An optional containing the symbol if found, or std::nullopt if not found.
 */
std::optional<Symbol> SymbolManager::findNearSymbol(edb::address_t address) const {

	auto it = symbolsByAddress_.lowerBound(address);
	if (it != symbolsByAddress_.end()) {

		// not an exact match, we should backup one
		if (address != it->address) {
			// not safe to backup!, return early
			if (it == symbolsByAddress_.begin()) {
				return std::nullopt;
			}
			--it;
		}

		const Symbol &sym = it.value();
		if (address >= sym.address && address < sym.address + sym.size) {
			return sym;
		}
	}

	return std::nullopt;
}

/**
 * @brief Adds a symbol to the symbol manager.
 *
 * @param symbol The symbol to add.
 */
void SymbolManager::addSymbol(const Symbol &symbol) {
	symbols_.push_back(symbol);
	symbolsByAddress_[symbol.address] = symbol;
	symbolsByName_[symbol.name]       = symbol;
	symbolsByFile_[symbol.file].push_back(symbol);
}

/**
 * @brief Processes a symbol file and loads its contents.
 *
 * @param f The path to the symbol file.
 * @param base The base address for the symbols.
 * @param library_filename The name of the library for which to load symbols.
 * @param allow_retry Whether to allow retrying if the symbol file is stale.
 * @return True if the symbol file was processed successfully, false otherwise.
 */
bool SymbolManager::processSymbolFile(const QString &f, edb::address_t base, const QString &library_filename, bool allow_retry) {

	// TODO(eteran): support filename starting with "http://" being fetched from a web server

	QFile symbolFile(f);
	if (symbolFile.size() == 0) {
		symbolFile.remove();
	}

	std::ifstream file(qPrintable(f));
	if (file) {
		edb::v1::set_status(tr("Loading symbols: %1").arg(f), 0);
		edb::address_t sym_start;
		edb::address_t sym_end;
		std::string sym_name;
		std::string date;
		std::string md5;
		std::string filename;

		if (std::getline(file, date)) {
			file >> md5 >> std::ws;
			std::getline(file, filename);
			if (file) {

				const QByteArray file_md5   = QByteArray::fromHex(md5.c_str());
				const QByteArray actual_md5 = edb::v1::get_file_md5(library_filename);

				if (file_md5 != actual_md5) {
					qDebug() << "Your symbol file for" << library_filename << "appears to not match the actual file, perhaps you should rebuild your symbols?";
					const Configuration &config = edb::v1::config();
					if (config.remove_stale_symbols) {
						symbolFile.remove();

						if (allow_retry) {
							return processSymbolFile(f, base, library_filename, false);
						}
					}
					edb::v1::clear_status();
					return false;
				}

				const QFileInfo info(QString::fromStdString(filename));
				const QString prefix = info.fileName();
				char sym_type;

				while (true) {
					file >> std::hex >> sym_start >> std::hex >> sym_end >> sym_type;
					// For symbol name we can't use operator>>() as it may have spaces if demangled
					// Thus, get the rest of the line as the symbol name
					std::getline(file, sym_name);

					if (!file) {
						if (!file.eof()) {
							qWarning() << "WARNING: File" << f << "seems corrupt";
						}
						break;
					}

					Symbol sym;

					sym.file           = f;
					sym.name_no_prefix = QString::fromStdString(sym_name).trimmed();
					sym.name           = QStringLiteral("%1!%2").arg(prefix, sym.name_no_prefix);
					sym.address        = sym_start;
					sym.size           = static_cast<uint32_t>(sym_end - sym_start);
					sym.type           = sym_type;

					// fixup the base address based on where it is loaded
					if (sym.address < base) {
						sym.address += base;
					}

					addSymbol(sym);
				}
				edb::v1::clear_status();
				return true;
			}
		}
	} else if (symbolGenerator_) {
		edb::v1::set_status(tr("Auto-Generating Symbol File: %1").arg(f), 0);
		bool generatedOK = symbolGenerator_->generateSymbolFile(library_filename, f);
		edb::v1::clear_status();
		if (generatedOK) {
			return false;
		}
	}

	// TODO(eteran): should we return false and try again later?
	edb::v1::clear_status();
	return true;
}

/**
 * @brief Returns a vector of all symbols currently managed by the symbol manager.
 *
 * @return A vector containing all symbols.
 */
std::vector<Symbol> SymbolManager::symbols() const {
	return symbols_;
}

/**
 * @brief Sets the symbol generator for the symbol manager.
 *
 * @param generator A pointer to the symbol generator to be used.
 */
void SymbolManager::setSymbolGenerator(ISymbolGenerator *generator) {
	symbolGenerator_ = generator;
}

/**
 * @brief Sets a label for a given address.
 *
 * A label is like a symbol, but can be set/unset by users. It will take
 * precedence over symbols (since this is the name that the user really
 * wants to call this address). And only apply to code
 *
 * @param address The address to label.
 * @param label The label to set.
 */
void SymbolManager::setLabel(edb::address_t address, const QString &label) {
	if (label.isEmpty()) {
		labelsByName_.remove(labels_[address]);
		labels_.remove(address);
	} else {

		if (labelsByName_.contains(label) && labelsByName_[label] != address) {
			QMessageBox::warning(
				edb::v1::debugger_ui,
				tr("Duplicate Label"),
				tr("You are attempting to give two separate addresses the same label, this is not supported."));
			return;
		}

		labels_[address]     = label;
		labelsByName_[label] = address;
	}
}

/**
 * @brief Finds the name of an address, either with or without a prefix.
 *
 * @param address The address to find.
 * @param prefixed Whether to include the prefix.
 * @return The name of the address.
 */
QString SymbolManager::findAddressName(edb::address_t address, bool prefixed) {
	auto it = labels_.find(address);
	if (it != labels_.end()) {
		return it.value();
	}

	if (const std::optional<Symbol> sym = find(address)) {
		return prefixed ? sym->name : sym->name_no_prefix;
	}

	return QString();
}

/**
 * @brief Gets the labels for all addresses.
 *
 * @return The labels for all addresses.
 */
QHash<edb::address_t, QString> SymbolManager::labels() const {
	return labels_;
}

/**
 * @brief Gets the list of all symbol files.
 *
 * @return The list of all symbol files.
 */
QStringList SymbolManager::files() const {
	return symbolsByFile_.keys();
}
