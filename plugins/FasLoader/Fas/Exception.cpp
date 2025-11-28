/*
 * @file: Exception.cpp
 *
 * This file part of RT ( Reconstructive Tools )
 * Copyright (c) 2018 darkprof <dark_prof@mail.ru>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 **/

#include "Exception.hpp"

namespace Fas {

Exception::Exception(std::string message)
	: message_(std::move(message)) {
}

const char *Exception::what() const noexcept {
	return message_.c_str();
}

}
