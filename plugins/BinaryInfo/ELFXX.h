/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef ELFXX_H_20070718_
#define ELFXX_H_20070718_

#include "IBinary.h"
#include "libELF/elf_binary.h"

namespace BinaryInfoPlugin {

template <class ElfHeader>
class ELFXX : public IBinary {
public:
	explicit ELFXX(const std::shared_ptr<IRegion> &region);
	~ELFXX() override = default;

public:
	[[nodiscard]] bool native() const override;
	[[nodiscard]] edb::address_t entryPoint() override;
	[[nodiscard]] size_t headerSize() const override;
	[[nodiscard]] const void *header() const override;
	[[nodiscard]] std::vector<Header> headers() const override;

private:
	void validateHeader();

private:
	std::shared_ptr<IRegion> region_;
	ElfHeader header_;
	edb::address_t baseAddress_{0};
	std::vector<Header> headers_;
};

using ELF32 = ELFXX<elf32_header>;
using ELF64 = ELFXX<elf64_header>;

}

#endif
