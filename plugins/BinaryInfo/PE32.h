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

#ifndef PE32_H_20070718_
#define PE32_H_20070718_

#include "IBinary.h"
#include "libPE/pe_binary.h"

namespace BinaryInfoPlugin {

class PEBinaryException : public std::exception {
public:
	enum Reason {
		INVALID_ARGUMENTS    = 1,
		READ_FAILURE         = 2,
		INVALID_PE           = 3,
		INVALID_ARCHITECTURE = 4
	};

public:
	explicit PEBinaryException(Reason reason);
	const char *what() const noexcept override;

private:
	Reason reason_;
};

class PE32 : public IBinary {
public:
	explicit PE32(const std::shared_ptr<IRegion> &region);
	~PE32() override = default;

public:
	bool native() const override;
	edb::address_t entryPoint() override;
	size_t headerSize() const override;
	const void *header() const override;
	std::vector<Header> headers() const override;

private:
	std::shared_ptr<IRegion> region_;
	libPE::IMAGE_DOS_HEADER dos_  = {};
	libPE::IMAGE_NT_HEADERS32 pe_ = {};
};

}

#endif
