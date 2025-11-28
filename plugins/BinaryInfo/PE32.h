/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
	[[nodiscard]] const char *what() const noexcept override;

private:
	Reason reason_;
};

class PE32 : public IBinary {
public:
	explicit PE32(std::shared_ptr<IRegion> region);
	~PE32() override = default;

public:
	[[nodiscard]] bool native() const override;
	[[nodiscard]] edb::address_t entryPoint() override;
	[[nodiscard]] size_t headerSize() const override;
	[[nodiscard]] const void *header() const override;
	[[nodiscard]] std::vector<Header> headers() const override;

private:
	std::shared_ptr<IRegion> region_;
	libPE::IMAGE_DOS_HEADER dos_  = {};
	libPE::IMAGE_NT_HEADERS32 pe_ = {};
};

}

#endif
