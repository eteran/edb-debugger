/*
Copyright (C) 2006 - 2015 Evan Teran
                          evan.teran@gmail.com

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
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

//------------------------------------------------------------------------------
// Name: clear
// Desc:
//------------------------------------------------------------------------------
void SymbolManager::clear() {
	symbolFiles_.clear();
	symbols_.clear();
	symbolsByAddress_.clear();
	symbolsByFile_.clear();
	symbolsByName_.clear();
	labels_.clear();
	labelsByName_.clear();
}

//------------------------------------------------------------------------------
// Name: loadSymbolFile
// Desc:
//------------------------------------------------------------------------------
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

		const QString path = QString("%1/%2").arg(symbol_directory, info.absolutePath());
		const QString name = info.fileName();

		// ensure that the sub-directory exists
		QDir().mkpath(path);

		if (!symbolFiles_.contains(info.absoluteFilePath())) {
			const QString map_file = QString("%1/%2.map").arg(path, name);

			if (processSymbolFile(map_file, base, filename, true)) {
				symbolFiles_.insert(info.absoluteFilePath());
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: find
// Desc:
//------------------------------------------------------------------------------
const std::shared_ptr<Symbol> SymbolManager::find(const QString &name) const {

	auto it = symbolsByName_.find(name);
	if (it != symbolsByName_.end()) {
		return it.value();
	}

	// slow path... look for any symbol which matches the name, but skipping the prefix
	// we can make this faster later at the cost of yet another hash table if we
	// feel the need
	auto it2 = std::find_if(symbols_.begin(), symbols_.end(), [&name](const std::shared_ptr<Symbol> &symbol) {
		return symbol->name_no_prefix == name;
	});

	if (it2 != symbols_.end()) {
		return *it2;
	}

	return nullptr;
}

//------------------------------------------------------------------------------
// Name: find
// Desc:
//------------------------------------------------------------------------------
const std::shared_ptr<Symbol> SymbolManager::find(edb::address_t address) const {
	auto it = symbolsByAddress_.find(address);
	return (it != symbolsByAddress_.end()) ? it.value() : nullptr;
}

//------------------------------------------------------------------------------
// Name: findNearSymbol
// Desc:
//------------------------------------------------------------------------------
const std::shared_ptr<Symbol> SymbolManager::findNearSymbol(edb::address_t address) const {

	auto it = symbolsByAddress_.lowerBound(address);
	if (it != symbolsByAddress_.end()) {

		// not an exact match, we should backup one
		if (address != it.value()->address) {
			// not safe to backup!, return early
			if (it == symbolsByAddress_.begin()) {
				return nullptr;
			}
			--it;
		}

		if (const std::shared_ptr<Symbol> sym = it.value()) {
			if (address >= sym->address && address < sym->address + sym->size) {
				return sym;
			}
		}
	}

	return nullptr;
}

//------------------------------------------------------------------------------
// Name: addSymbol
// Desc:
//------------------------------------------------------------------------------
void SymbolManager::addSymbol(const std::shared_ptr<Symbol> &symbol) {
	Q_ASSERT(symbol);
	symbols_.push_back(symbol);
	symbolsByAddress_[symbol->address] = symbol;
	symbolsByName_[symbol->name]       = symbol;
	symbolsByFile_[symbol->file].push_back(symbol);
}

//------------------------------------------------------------------------------
// Name: processSymbolFile
// Desc:
// Note: returning false means 'try again', true means, 'we loaded what we could'
//------------------------------------------------------------------------------
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
						if (!file.eof()) qWarning() << "WARNING: File" << f << "seems corrupt";
						break;
					}

					auto sym = std::make_shared<Symbol>();

					sym->file           = f;
					sym->name_no_prefix = QString::fromStdString(sym_name).trimmed();
					sym->name           = QString("%1!%2").arg(prefix, sym->name_no_prefix);
					sym->address        = sym_start;
					sym->size           = sym_end;
					sym->type           = sym_type;

					// fixup the base address based on where it is loaded
					if (sym->address < base) {
						sym->address += base;
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

//------------------------------------------------------------------------------
// Name: symbols
// Desc:
//------------------------------------------------------------------------------
const std::vector<std::shared_ptr<Symbol>> SymbolManager::symbols() const {
	return symbols_;
}

//------------------------------------------------------------------------------
// Name: setSymbolGenerator
// Desc:
//------------------------------------------------------------------------------
void SymbolManager::setSymbolGenerator(ISymbolGenerator *generator) {
	symbolGenerator_ = generator;
}

//------------------------------------------------------------------------------
// Name: setLabel
// Desc: a label is like a symbol, but can be set/unset by users. They will take
//       precidence over symbols (since this is the name that the user really
//       wants to call this address). And only apply to code
//------------------------------------------------------------------------------
void SymbolManager::setLabel(edb::address_t address, const QString &label) {
	if (label.isEmpty()) {
		labelsByName_.remove(labels_[address]);
		labels_.remove(address);
	} else {

		if (labelsByName_.contains(label) && labelsByName_[label] != address) {
			QMessageBox::warning(
				edb::v1::debugger_ui,
				tr("Duplicate Label"),
				tr("You are attempting to give two seperate addresses the same label, this is not supported."));
			return;
		}

		labels_[address]     = label;
		labelsByName_[label] = address;
	}
}

//------------------------------------------------------------------------------
// Name: findAddressName
// Desc:
//------------------------------------------------------------------------------
QString SymbolManager::findAddressName(edb::address_t address, bool prefixed) {
	auto it = labels_.find(address);
	if (it != labels_.end()) {
		return it.value();
	}

	if (const std::shared_ptr<Symbol> sym = find(address)) {
		return prefixed ? sym->name : sym->name_no_prefix;
	}

	return QString();
}

//------------------------------------------------------------------------------
// Name: labels
// Desc:
//------------------------------------------------------------------------------
QHash<edb::address_t, QString> SymbolManager::labels() const {
	return labels_;
}

//------------------------------------------------------------------------------
// Name: files
// Desc:
//------------------------------------------------------------------------------
QStringList SymbolManager::files() const {
	return symbolsByFile_.keys();
}
