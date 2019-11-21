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
	bool native() const override;
	edb::address_t entryPoint() override;
	size_t headerSize() const override;
	const void *header() const override;
	std::vector<Header> headers() const override;

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
