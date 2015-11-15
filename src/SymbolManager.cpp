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
#include "ISymbolGenerator.h"
#include "MD5.h"
#include "edb.h"
#include "Configuration.h"

#include <QFile>
#include <QDir>
#include <QtDebug>
#include <QProcess>
#include <QMessageBox>
#include <istream>
#include <fstream>
#include <iostream>

//------------------------------------------------------------------------------
// Name: SymbolManager
// Desc:
//------------------------------------------------------------------------------
SymbolManager::SymbolManager() : symbol_generator_(0), show_path_notice_(true) {
}


//------------------------------------------------------------------------------
// Name: clear
// Desc:
//------------------------------------------------------------------------------
void SymbolManager::clear() {
	symbol_files_.clear();
	symbols_.clear();
	symbols_by_address_.clear();
	symbols_by_file_.clear();
	symbols_by_name_.clear();
	labels_.clear();
	labels_by_name_.clear();
}

//------------------------------------------------------------------------------
// Name: load_symbol_file
// Desc:
//------------------------------------------------------------------------------
void SymbolManager::load_symbol_file(const QString &filename, edb::address_t base) {

	const QString symbol_directory = edb::v1::config().symbol_path;

	if(symbol_directory.isEmpty()) {
		if(show_path_notice_) {
			qDebug() << "No symbol path specified. Please set it in the preferences to enable symbols.";
			show_path_notice_ = false;
		}
		return;
	}

	if(!symbol_directory.isEmpty()) {
		QDir symbolPath;
		symbolPath.mkpath(symbol_directory);
	}

	const QFileInfo info(filename);
	const QString name = info.fileName();

	if(!symbol_files_.contains(name)) {
		const QString map_file = QString("%1/%2.map").arg(symbol_directory, name);

		if(process_symbol_file(map_file, base, filename, true)) {
			symbol_files_.insert(name);
		}
	}
}

//------------------------------------------------------------------------------
// Name: find
// Desc:
//------------------------------------------------------------------------------
const Symbol::pointer SymbolManager::find(const QString &name) const {
	auto it = symbols_by_name_.find(name);
	if(it != symbols_by_name_.end()) {
		return it.value();
	}
	return Symbol::pointer();
}

//------------------------------------------------------------------------------
// Name: find
// Desc:
//------------------------------------------------------------------------------
const Symbol::pointer SymbolManager::find(edb::address_t address) const {
	auto it = symbols_by_address_.find(address);
	return (it != symbols_by_address_.end()) ? it.value() : Symbol::pointer();
}

//------------------------------------------------------------------------------
// Name: find_near_symbol
// Desc:
//------------------------------------------------------------------------------
const Symbol::pointer SymbolManager::find_near_symbol(edb::address_t address) const {

	auto it = symbols_by_address_.lowerBound(address);
	if(it != symbols_by_address_.end()) {

		// not an exact match, we should backup one
		if(address != it.value()->address) {
			// not safe to backup!, return early
			if(it == symbols_by_address_.begin()) {
				return Symbol::pointer();
			}
			--it;
		}

		if(const Symbol::pointer sym = it.value()) {
			if(address >= sym->address && address < sym->address + sym->size) {
				return sym;
			}
		}
	}

	return Symbol::pointer();
}

//------------------------------------------------------------------------------
// Name: add_symbol
// Desc:
//------------------------------------------------------------------------------
void SymbolManager::add_symbol(const Symbol::pointer &symbol) {
	Q_ASSERT(symbol);
	symbols_.push_back(symbol);
	symbols_by_address_[symbol->address] = symbol;
	symbols_by_name_[symbol->name]       = symbol;
	symbols_by_file_[symbol->file].push_back(symbol);
}

//------------------------------------------------------------------------------
// Name: process_symbol_file
// Desc:
// Note: returning false means 'try again', true means, 'we loaded what we could'
//------------------------------------------------------------------------------
bool SymbolManager::process_symbol_file(const QString &f, edb::address_t base, const QString &library_filename, bool allow_retry) {

	// TODO(eteran): support filename starting with "http://" being fetched from a web server
	// TODO(eteran): support symbol files with paths so we can deal with binaries that have
	//       conflicting names in different directories

	std::ifstream file(qPrintable(f));
	if(file) {
		qDebug() << "loading symbols:" << f;
		edb::address_t sym_start;
		edb::address_t sym_end;
		std::string    sym_name;
		std::string    date;
		std::string    md5;
		std::string    filename;

		if(std::getline(file, date)) {
			if(file >> md5 >> filename) {

				const QByteArray file_md5   = QByteArray::fromHex(md5.c_str());
				const QByteArray actual_md5 = edb::v1::get_file_md5(library_filename);

				if(file_md5 != actual_md5) {
					qDebug() << "Your symbol file for" << library_filename << "appears to not match the actual file, perhaps you should rebuild your symbols?";
					const Configuration &config = edb::v1::config();
					if(config.remove_stale_symbols) {
						QFile symbolFile(f);
						symbolFile.remove();

						if(allow_retry) {
							return process_symbol_file(f, base, library_filename, false);
						}

					}
					return false;
				}

				const QFileInfo info(QString::fromStdString(filename));
				const QString prefix = info.fileName();
				char sym_type;

				while(true) {
					file >> std::hex >> sym_start >> std::hex >> sym_end >> sym_type;
					// For symbol name we can't use operator>>() as it may have spaces if demangled
					// Thus, get the rest of the line as the symbol name
					std::getline(file,sym_name);

					if(!file) {
						if(!file.eof()) qWarning() << "WARNING: File" << f << "seems corrupt";
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
					if(sym->address < base) {
						sym->address += base;
					}

					add_symbol(sym);
				}
				return true;
			}
		}
	} else if(symbol_generator_) {
		qDebug() << "Auto-Generating Symbol File: " << f;
		if(symbol_generator_->generate_symbol_file(library_filename, f)) {
			return false;
		}
	}

	// TODO: should we return false and try again later?
	return true;
}

//------------------------------------------------------------------------------
// Name: symbols
// Desc:
//------------------------------------------------------------------------------
const QList<Symbol::pointer> SymbolManager::symbols() const {
	return symbols_;
}

//------------------------------------------------------------------------------
// Name: set_symbol_generator
// Desc:
//------------------------------------------------------------------------------
void SymbolManager::set_symbol_generator(ISymbolGenerator *generator) {
	symbol_generator_ = generator;
}

//------------------------------------------------------------------------------
// Name: set_label
// Desc: a label is like a symbol, but can be set/unset by users. They will take
//       precidence over symbols (since this is the name that the user really
//       wants to call this address). And only apply to code
//------------------------------------------------------------------------------
void SymbolManager::set_label(edb::address_t address, const QString &label) {
	if(label.isEmpty()) {
		labels_by_name_.remove(labels_[address]);
		labels_.remove(address);
	} else {

		if(labels_by_name_.contains(label) && labels_by_name_[label] != address) {
			QMessageBox::warning(
				edb::v1::debugger_ui,
				QT_TRANSLATE_NOOP("edb", "Duplicate Label"),
				QT_TRANSLATE_NOOP("edb", "You are attempting to give two seperate addresses the same label, this is not supported.")
				);
			return;
		}

		labels_[address] = label;
		labels_by_name_[label] = address;
	}
}

//------------------------------------------------------------------------------
// Name: find_address_name
// Desc:
//------------------------------------------------------------------------------
QString SymbolManager::find_address_name(edb::address_t address) {
	auto it = labels_.find(address);
	if(it != labels_.end()) {
		return it.value();
	}

	if(const Symbol::pointer sym = find(address)) {
		return sym->name;
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
QList<QString> SymbolManager::files() const {
	return symbols_by_file_.keys();
}
