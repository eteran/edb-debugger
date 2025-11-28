/*
 * @file: Exception.hpp
 *
 * This file part of RT ( Reconstructive Tools )
 * Copyright (c) 2018 darkprof <dark_prof@mail.ru>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 **/

#ifndef FAS_EXCEPTION_H_
#define FAS_EXCEPTION_H_

#include <exception>
#include <string>

namespace Fas {

class Exception : public std::exception {
public:
	explicit Exception(std::string message);
	~Exception() noexcept override = default;

	[[nodiscard]] const char *what() const noexcept override;

protected:
	std::string message_;
};

}

#endif
