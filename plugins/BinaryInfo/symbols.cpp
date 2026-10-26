/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "symbols.h"
#include "demangle.h"
#include "edb.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <set>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <vector>

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QSettings>
#include <QString>

#include "libELF/elf_header.h"
#include "libELF/elf_model.h"
#include "libELF/elf_rel.h"
#include "libELF/elf_rela.h"
#include "libELF/elf_shdr.h"
#include "libELF/elf_sym.h"
#include "libELF/elf_syminfo.h"
#include "libELF/elf_types.h"

namespace BinaryInfoPlugin {
namespace {

class MemoryReader {
public:
	MemoryReader(const void *data, uint64_t size) : data_(static_cast<const char *>(data)), size_(size) {
		assert(data_ != nullptr || size_ == 0);
	}

public:
	[[nodiscard]] uint64_t size() const { return size_; }

public:
	[[nodiscard]] bool contains(uint64_t offset, uint64_t length) const {
		return offset <= size_ &&
			   length <= size_ - offset &&
			   offset <= std::numeric_limits<size_t>::max() &&
			   length <= std::numeric_limits<size_t>::max() - static_cast<size_t>(offset);
	}

	[[nodiscard]] std::optional<std::string_view> view(uint64_t offset, uint64_t length) const {
		if (!contains(offset, length)) {
			return std::nullopt;
		}

		if (length == 0) {
			return std::string_view{};
		}

		return std::string_view(data_ + static_cast<size_t>(offset), static_cast<size_t>(length));
	}

	[[nodiscard]] bool read(void *buffer, uint64_t offset, uint64_t length) const {
		if (!contains(offset, length)) {
			return false;
		}

		if (length == 0) {
			return true;
		}

		if (buffer == nullptr) {
			return false;
		}

		std::memcpy(buffer, data_ + static_cast<size_t>(offset), static_cast<size_t>(length));
		return true;
	}

private:
	const char *data_ = nullptr;
	uint64_t size_    = 0;
};

struct elf32_model : elf_model<32> {

	static constexpr size_t plt_entry_size = 0x10;

	static constexpr uint32_t elf_r_sym(uint32_t x) { return ELF32_R_SYM(x); }
	static constexpr uint32_t elf_r_type(uint32_t x) { return ELF32_R_TYPE(x); }
	static constexpr uint8_t elf_st_type(uint8_t x) { return ELF32_ST_TYPE(x); }
	static constexpr uint8_t elf_st_bind(uint8_t x) { return static_cast<uint8_t>(ELF32_ST_BIND(x)); }

	struct symbol {
		elf_addr address;
		size_t size;
		QString name;
		char type;

		[[nodiscard]] bool operator<(const symbol &rhs) const {
			return std::tie(address, name) < std::tie(rhs.address, rhs.name);
		}

		[[nodiscard]] bool operator==(const symbol &rhs) const {
			return std::tie(address, name) == std::tie(rhs.address, rhs.name);
		}

		[[nodiscard]] QString to_string() const {
			return QStringLiteral("%1 %2 %3 %4").arg(edb::value32(address).toHexString(), edb::value32(size).toHexString()).arg(type).arg(name);
		}
	};
};

struct elf64_model : elf_model<64> {

	static constexpr size_t plt_entry_size = 0x10;

	static constexpr uint64_t elf_r_sym(uint64_t x) { return ELF64_R_SYM(x); }
	static constexpr uint64_t elf_r_type(uint64_t x) { return ELF64_R_TYPE(x); }
	static constexpr uint8_t elf_st_type(uint8_t x) { return ELF64_ST_TYPE(x); }
	static constexpr uint8_t elf_st_bind(uint8_t x) { return static_cast<uint8_t>(ELF64_ST_BIND(x)); }

	struct symbol {
		elf_addr address;
		size_t size;
		QString name;
		char type;

		[[nodiscard]] bool operator<(const symbol &rhs) const {
			return std::tie(address, name) < std::tie(rhs.address, rhs.name);
		}

		[[nodiscard]] bool operator==(const symbol &rhs) const {
			return std::tie(address, name) == std::tie(rhs.address, rhs.name);
		}

		[[nodiscard]] QString to_string() const {
			return QStringLiteral("%1 %2 %3 %4").arg(edb::value64(address).toHexString(), edb::value64(size).toHexString()).arg(type).arg(name);
		}
	};
};

template <typename T>
[[nodiscard]] bool read_table(const MemoryReader &reader, uint64_t offset, uint64_t count, uint64_t entry_size, std::vector<T> &table) {
	if (entry_size != sizeof(T) || count > table.max_size()) {
		return false;
	}

	const uint64_t table_size = count * entry_size;
	if (!reader.contains(offset, table_size)) {
		return false;
	}

	table.resize(static_cast<size_t>(count));
	return reader.read(table.data(), offset, table_size);
}

template <typename T, typename Section>
[[nodiscard]] bool read_section_table(const MemoryReader &reader, const Section &section, std::vector<T> &table) {
	if (section.sh_type == SHT_NOBITS || section.sh_entsize == 0 || section.sh_size % section.sh_entsize != 0) {
		return false;
	}
	return read_table(reader, section.sh_offset, section.sh_size / section.sh_entsize, section.sh_entsize, table);
}

template <typename Section>
[[nodiscard]] std::optional<std::string_view> get_string(const MemoryReader &reader, const Section &section, uint64_t offset) {
	if (section.sh_type != SHT_STRTAB) {
		return std::nullopt;
	}

	const auto section_data = reader.view(section.sh_offset, section.sh_size);
	if (!section_data || offset >= section_data->size()) {
		return std::nullopt;
	}

	const char *begin        = section_data->data() + static_cast<size_t>(offset);
	const auto remaining     = section_data->size() - static_cast<size_t>(offset);
	const auto terminator    = static_cast<const char *>(std::memchr(begin, '\0', remaining));
	if (terminator == nullptr) {
		return std::nullopt;
	}

	return std::string_view(begin, static_cast<size_t>(terminator - begin));
}

[[nodiscard]] bool make_qstring(std::string_view text, QString &value) {
	if (text.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
		return false;
	}
	value = QString::fromLocal8Bit(text.data(), static_cast<int>(text.size()));
	return true;
}

[[nodiscard]] bool valid_elf_ident(const MemoryReader &reader, uint8_t elf_class) {

	uint8_t ident[EI_NIDENT] = {0};
	if (!reader.read(ident, 0, sizeof(ident))) {
		return false;
	}

	uint16_t endian_probe         = 1;
	const uint8_t native_encoding = *reinterpret_cast<const uint8_t *>(&endian_probe) == 1 ? ELFDATA2LSB : ELFDATA2MSB;
	return std::memcmp(ident, ELFMAG, SELFMAG) == 0 &&
		   ident[EI_CLASS] == elf_class &&
		   ident[EI_DATA] == native_encoding &&
		   ident[EI_VERSION] == EV_CURRENT;
}

[[nodiscard]] bool is_elf32(const MemoryReader &reader) {
	return valid_elf_ident(reader, ELFCLASS32) && reader.size() >= sizeof(elf32_header);
}

[[nodiscard]] bool is_elf64(const MemoryReader &reader) {
	return valid_elf_ident(reader, ELFCLASS64) && reader.size() >= sizeof(elf64_header);
}

/*
The  symbol  type.   At least the following types are used; others are, as well, depending on the object file format.  If lowercase,
the symbol is local; if uppercase, the symbol is global (external).

"A" The symbol's value is absolute, and will not be changed by further linking.

"B"
"b" The symbol is in the uninitialized data section (known as BSS).

"C" The symbol is common.  Common symbols are uninitialized data.  When linking, multiple common symbols may appear  with  the  same
	name.  If the symbol is defined anywhere, the common symbols are treated as undefined references.

"D"
"d" The symbol is in the initialized data section.

"G"
"g" The  symbol is in an initialized data section for small objects.  Some object file formats permit more efficient access to small
	data objects, such as a global int variable as opposed to a large global array.

"N" The symbol is a debugging symbol.

"p" The symbols is in a stack unwind section.

"R"
"r" The symbol is in a read only data section.

"S"
"s" The symbol is in an uninitialized data section for small objects.

"T"
"t" The symbol is in the text (code) section.

"U" The symbol is undefined.

"u" The  symbol  is  a unique global symbol.  This is a GNU extension to the standard set of ELF symbol bindings.  For such a symbol
	the dynamic linker will make sure that in the entire process there is just one symbol with this name and type in use.

"V"
"v" The symbol is a weak object.  When a weak defined symbol is linked with a normal defined symbol, the normal  defined  symbol  is
	used  with no error.  When a weak undefined symbol is linked and the symbol is not defined, the value of the weak symbol becomes
	zero with no error.  On some systems, uppercase indicates that a default value has been specified.

"W"
"w" The symbol is a weak symbol that has not been specifically tagged as a weak object symbol.  When a weak defined symbol is linked
	with  a  normal defined symbol, the normal defined symbol is used with no error.  When a weak undefined symbol is linked and the
	symbol is not defined, the value of the symbol is determined in a system-specific manner without error.  On some systems, upper-
	case indicates that a default value has been specified.

"-" The  symbol  is  a  stabs  symbol in an a.out object file.  In this case, the next values printed are the stabs other field, the
	stabs desc field, and the stab type.  Stabs symbols are used to hold debugging information.

"?" The symbol type is unknown, or object file format specific.
*/

template <class M>
[[nodiscard]] bool collect_symbols(const MemoryReader &reader, std::vector<typename M::symbol> &symbols) {

	using elf_addr   = typename M::elf_addr;
	using elf_header = typename M::elf_header;
	using elf_shdr   = typename M::elf_shdr;
	using elf_sym    = typename M::elf_sym;
	using elf_rela   = typename M::elf_rela;
	using elf_rel    = typename M::elf_rel;
	using symbol     = typename M::symbol;

	elf_header header{};
	if (!reader.read(&header, 0, sizeof(header))) {
		return false;
	}

	if (header.e_ehsize != sizeof(elf_header) ||
		header.e_version != EV_CURRENT ||
		header.e_shentsize != sizeof(elf_shdr) ||
		header.e_shoff == 0) {
		return false;
	}

	elf_shdr section_zero{};
	if (!reader.read(&section_zero, header.e_shoff, sizeof(section_zero))) {
		return false;
	}

	const uint64_t section_count       = header.e_shnum != 0 ? header.e_shnum : section_zero.sh_size;
	const uint64_t section_names_index = header.e_shstrndx == SHN_XINDEX ? section_zero.sh_link : header.e_shstrndx;
	if (section_count == 0 || (section_names_index != SHN_UNDEF && section_names_index >= section_count)) {
		return false;
	}

	std::vector<elf_shdr> sections;
	if (!read_table(reader, header.e_shoff, section_count, header.e_shentsize, sections)) {
		return false;
	}

	const bool has_section_names = section_names_index != SHN_UNDEF;
	std::vector<std::string_view> section_names_cache(sections.size());
	if (has_section_names) {
		const elf_shdr &section_names = sections[static_cast<size_t>(section_names_index)];
		if (section_names.sh_type != SHT_STRTAB) {
			return false;
		}

		for (size_t i = 0; i < sections.size(); ++i) {
			const auto name = get_string(reader, section_names, sections[i].sh_name);
			if (!name) {
				return false;
			}
			section_names_cache[i] = *name;
		}
	}

	std::vector<symbol> collected_symbols;

	elf_addr plt_address     = 0;
	elf_addr plt_sec_address = 0;
	elf_addr got_address     = 0;
	std::set<elf_addr> plt_addresses;

	// collect special section addresses
	for (size_t i = 0; i < sections.size(); ++i) {
		if (section_names_cache[i] == ".plt") {
			plt_address = sections[i].sh_addr;
		} else if (section_names_cache[i] == ".got") {
			got_address = sections[i].sh_addr;
		} else if (section_names_cache[i] == ".plt.sec") {
			plt_sec_address = sections[i].sh_addr;
		}
	}

	// print out relocated symbols for special sections
	for (size_t section_index = 0; section_index < sections.size(); ++section_index) {
		const auto &section = sections[section_index];

		elf_addr base_address = 0;
		if (section_names_cache[section_index] == ".rela.plt") {
			base_address = plt_sec_address ? plt_sec_address : plt_address;
		} else if (section_names_cache[section_index] == ".rel.plt") {
			base_address = plt_sec_address ? plt_sec_address : plt_address;
		} else if (section_names_cache[section_index] == ".rela.got") {
			base_address = got_address;
		} else if (section_names_cache[section_index] == ".rel.got") {
			base_address = got_address;
		} else {
			continue;
		}

		switch (section.sh_type) {
		case SHT_RELA: {
			if (section.sh_link >= sections.size()) {
				return false;
			}
			const auto &linked = sections[section.sh_link];
			if (linked.sh_type != SHT_SYMTAB && linked.sh_type != SHT_DYNSYM) {
				return false;
			}
			std::vector<elf_rela> relocations;
			std::vector<elf_sym> symbol_table;
			if (!read_section_table(reader, section, relocations) ||
				!read_section_table(reader, linked, symbol_table) ||
				linked.sh_link >= sections.size()) {
				return false;
			}
			const auto &strings = sections[linked.sh_link];
			if (strings.sh_type != SHT_STRTAB || !reader.contains(strings.sh_offset, strings.sh_size)) {
				return false;
			}

			elf_addr n = 0;
			if (!plt_sec_address) {
				++n;
			}

			for (const auto &relocation : relocations) {
				const size_t sym_index = static_cast<size_t>(M::elf_r_sym(relocation.r_info));
				if (sym_index >= symbol_table.size()) {
					return false;
				}

				const auto &elf_symbol = symbol_table[sym_index];
				const auto symbol_name = get_string(reader, strings, elf_symbol.st_name);
				if (!symbol_name ||
					n > (std::numeric_limits<elf_addr>::max() - base_address) / M::plt_entry_size) {
					return false;
				}

				const auto symbol_address        = static_cast<elf_addr>(base_address + (n * M::plt_entry_size));
				std::string_view relocation_name = section_names_cache[section_index];
				relocation_name.remove_prefix(sizeof(".rela.") - 1);
				symbol sym;
				sym.address = symbol_address;
				sym.size    = (elf_symbol.st_size ? elf_symbol.st_size : 0x10);
				if (!make_qstring(*symbol_name, sym.name) || relocation_name.size() > static_cast<size_t>(std::numeric_limits<int>::max() - sym.name.size())) {
					return false;
				}
				sym.name += QLatin1Char('@');
				QString suffix;
				if (!make_qstring(relocation_name, suffix)) {
					return false;
				}
				sym.name += suffix;
				sym.type = 'P';
				collected_symbols.push_back(sym);
				plt_addresses.insert(symbol_address);
				++n;
			}
		} break;
		case SHT_REL: {
			if (section.sh_link >= sections.size()) {
				return false;
			}
			const auto &linked = sections[section.sh_link];
			if (linked.sh_type != SHT_SYMTAB && linked.sh_type != SHT_DYNSYM) {
				return false;
			}
			std::vector<elf_rel> relocations;
			std::vector<elf_sym> symbol_table;
			if (!read_section_table(reader, section, relocations) ||
				!read_section_table(reader, linked, symbol_table) ||
				linked.sh_link >= sections.size()) {
				return false;
			}
			const auto &strings = sections[linked.sh_link];
			if (strings.sh_type != SHT_STRTAB || !reader.contains(strings.sh_offset, strings.sh_size)) {
				return false;
			}

			elf_addr n = 0;
			if (!plt_sec_address) {
				++n;
			}

			for (const auto &relocation : relocations) {
				const size_t sym_index = static_cast<size_t>(M::elf_r_sym(relocation.r_info));
				if (sym_index >= symbol_table.size()) {
					return false;
				}

				const auto &elf_symbol = symbol_table[sym_index];
				const auto symbol_name = get_string(reader, strings, elf_symbol.st_name);
				if (!symbol_name ||
					n > (std::numeric_limits<elf_addr>::max() - base_address) / M::plt_entry_size) {
					return false;
				}

				const auto symbol_address        = static_cast<elf_addr>(base_address + (n * M::plt_entry_size));
				std::string_view relocation_name = section_names_cache[section_index];
				relocation_name.remove_prefix(sizeof(".rel.") - 1);
				symbol sym;
				sym.address = symbol_address;
				sym.size    = (elf_symbol.st_size ? elf_symbol.st_size : 0x10);
				if (!make_qstring(*symbol_name, sym.name)) {
					return false;
				}
				sym.name += QLatin1Char('@');
				QString suffix;
				if (!make_qstring(relocation_name, suffix)) {
					return false;
				}
				sym.name += suffix;
				sym.type = 'P';
				collected_symbols.push_back(sym);
				plt_addresses.insert(symbol_address);
				++n;
			}
		} break;
		}
	}

	// collect regular and unnamed symbols
	for (const auto &section : sections) {
		switch (section.sh_type) {
		case SHT_SYMTAB:
		case SHT_DYNSYM: {
			if (section.sh_link >= sections.size()) {
				return false;
			}
			std::vector<elf_sym> symbol_table;
			if (!read_section_table(reader, section, symbol_table)) {
				return false;
			}
			const auto &strings = sections[section.sh_link];
			if (strings.sh_type != SHT_STRTAB || !reader.contains(strings.sh_offset, strings.sh_size)) {
				return false;
			}

			for (const auto &elf_symbol : symbol_table) {
				if (plt_addresses.find(elf_symbol.st_value) == plt_addresses.end() && elf_symbol.st_value != 0) {
					const auto name = get_string(reader, strings, elf_symbol.st_name);
					if (!name) {
						return false;
					}
					if (!name->empty()) {
						symbol sym;
						sym.address = elf_symbol.st_value;
						sym.size    = elf_symbol.st_size;
						if (!make_qstring(*name, sym.name)) {
							return false;
						}
						sym.type = (M::elf_st_type(elf_symbol.st_info) == STT_FUNC ? 'T' : 'D');
						collected_symbols.push_back(sym);
					} else {
						symbol sym;
						sym.address = elf_symbol.st_value;
						sym.size    = elf_symbol.st_size;

						if (has_section_names) {
							for (size_t i = 0; i < sections.size(); ++i) {
								const auto &candidate = sections[i];
								if (sym.address >= candidate.sh_addr) {
									const auto offset = sym.address - candidate.sh_addr;
									if (offset <= candidate.sh_size && sym.size <= candidate.sh_size - offset) {
										const QString hexPrefix = offset > 9 ? QStringLiteral("0x") : QString();
										const QString offsetStr = offset ? QStringLiteral("+") + hexPrefix + QString::number(offset, 16) : QString();
										QString section_name;
										if (!make_qstring(section_names_cache[i], section_name)) {
											return false;
										}
										if (!section_name.isEmpty()) {
											sym.name = QString(section_name + offsetStr);
											break;
										}
									}
								}
							}
						}

						if (sym.name.isEmpty()) {
							sym.name = QStringLiteral("$sym_%1").arg(edb::v1::format_pointer(elf_symbol.st_value));
						}

						sym.type = (M::elf_st_type(elf_symbol.st_info) == STT_FUNC ? 'T' : 'D');
						collected_symbols.push_back(sym);
					}
				}
			}
		} break;
		}
	}
	symbols.insert(symbols.end(), collected_symbols.begin(), collected_symbols.end());
	return true;
}

template <class M>
[[nodiscard]] bool try_collect_symbols(const MemoryReader &reader, std::vector<typename M::symbol> &symbols) {
	try {
		return collect_symbols<M>(reader, symbols);
	} catch (const std::bad_alloc &) {
		qWarning() << "Not enough memory to parse ELF symbols";
		return false;
	} catch (const std::length_error &) {
		qWarning() << "ELF symbol table is too large";
		return false;
	}
}

/**
 * @brief Outputs the symbols to the specified stream ensuring uniqueness and adding any needed demangling.
 *
 * @param symbols The vector of symbols to output.
 * @param os The output stream to write the symbols to.
 */
template <class Symbol>
void output_symbols(std::vector<Symbol> &symbols, std::ostream &os) {
	std::sort(symbols.begin(), symbols.end());
	auto new_end                 = std::unique(symbols.begin(), symbols.end());
	const auto demanglingEnabled = QSettings().value(QStringLiteral("BinaryInfo/demangling_enabled"), true).toBool();
	for (auto it = symbols.begin(); it != new_end; ++it) {
		if (demanglingEnabled) {
			it->name = demangle(it->name);
		}
		os << qPrintable(it->to_string()) << '\n';
	}
}

/**
 * @brief Generates symbols for the given file and outputs them to the specified stream.
 *
 * @param file The file to generate symbols for.
 * @param debugFile The debug file to include in the symbol generation.
 * @param os The output stream to write the symbols to.
 * @return True if the operation was successful, false otherwise.
 */
bool generate_symbols_internal(QFile &file, std::optional<QFile> &debugFile, std::ostream &os) {
	if (auto file_ptr = static_cast<void *>(file.map(0, file.size(), QFile::NoOptions))) {
		const auto file_size = static_cast<uint64_t>(file.size());

		MemoryReader reader(file_ptr, file_size);

		if (is_elf64(reader)) {

			using symbol = typename elf64_model::symbol;
			std::vector<symbol> symbols;

			if (!try_collect_symbols<elf64_model>(reader, symbols)) {
				return false;
			}

			// if there was a debug file
			// and we successfully opened it
			if (debugFile && debugFile->open(QIODevice::ReadOnly)) {

				// map it and include it with the symbols
				if (auto debug_ptr = static_cast<void *>(debugFile->map(0, debugFile->size(), QFile::NoOptions))) {

					MemoryReader debug_reader(debug_ptr, static_cast<uint64_t>(debugFile->size()));

					// this should never fail... but just being sure
					if (is_elf64(debug_reader)) {
						if (!try_collect_symbols<elf64_model>(debug_reader, symbols)) {
							qWarning() << "Invalid ELF debug file:" << debugFile->fileName();
						}
					}
				}
			}

			output_symbols(symbols, os);
			return true;
		}

		if (is_elf32(reader)) {

			using symbol = typename elf32_model::symbol;
			std::vector<symbol> symbols;

			if (!try_collect_symbols<elf32_model>(reader, symbols)) {
				return false;
			}

			// if there was a debug file
			// and we successfully opened it
			if (debugFile && debugFile->open(QIODevice::ReadOnly)) {

				// map it and include it with the symbols
				if (auto debug_ptr = static_cast<void *>(debugFile->map(0, debugFile->size(), QFile::NoOptions))) {

					MemoryReader debug_reader(debug_ptr, static_cast<uint64_t>(debugFile->size()));

					// this should never fail... but just being sure
					if (is_elf32(debug_reader)) {
						if (!try_collect_symbols<elf32_model>(debug_reader, symbols)) {
							qWarning() << "Invalid ELF debug file:" << debugFile->fileName();
						}
					}
				}
			}

			output_symbols(symbols, os);
			return true;
		}

		qDebug("unknown file type");
	}

	return false;
}

}

/**
 * @brief Generates symbols for the given file and outputs them to the specified stream.
 *
 * @param filename The name of the file to generate symbols for.
 * @param os The output stream to write the symbols to.
 * @return True if the operation was successful, false otherwise.
 */
bool generate_symbols(const QString &filename, std::ostream &os) {

	QFile file(filename);
	if (file.open(QIODevice::ReadOnly)) {
		os << qPrintable(QDateTime::currentDateTimeUtc().toString(Qt::ISODate)) << " +0000" << '\n';
		const QByteArray md5 = edb::v1::get_file_md5(filename);
		os << md5.toHex().data() << ' ' << qPrintable(QFileInfo(filename).absoluteFilePath()) << '\n';

		const QString debugInfoPath = QSettings().value(QStringLiteral("BinaryInfo/debug_info_path"), QStringLiteral("/usr/lib/debug")).toString();

		std::optional<QFile> debugFile;
		if (!debugInfoPath.isEmpty()) {
			debugFile.emplace(QStringLiteral("%1/%2.debug").arg(debugInfoPath, filename));
			if (!debugFile->exists()) { // systems such as Ubuntu don't have .debug suffix, try without it
				debugFile.emplace(QStringLiteral("%1/%2").arg(debugInfoPath, filename));
			}
		}

		return generate_symbols_internal(file, debugFile, os);
	}

	return false;
}

}
