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

#include "Register.h"
#include "Util.h"

/**
 * @brief Register::Register
 */
Register::Register() {
	util::mark_memory(&value_, sizeof(value_));
}

/**
 * @brief Register::operator ==
 * @param rhs
 * @return
 */
bool Register::operator==(const Register &rhs) const {
	if (!valid() && !rhs.valid()) {
		return true;
	} else {
		return name_ == rhs.name_ && value_ == rhs.value_ && type_ == rhs.type_ && bitSize_ == rhs.bitSize_;
	}
}

/**
 * @brief Register::toHexString
 * @return
 */
QString Register::toHexString() const {
	if (!valid()) {
		return tr("<undefined>");
	}

	if (bitSize_ % 8) {
		return tr("(Error: bad register length %1 bits)").arg(bitSize_);
	}

	return value_.toHexString().right(bitSize_ / 4); // TODO: trimming should be moved to valueXX::toHexString()
}

/**
 * @brief Register::operator !=
 * @param rhs
 * @return
 */
bool Register::operator!=(const Register &rhs) const {
	return !(*this == rhs);
}

/**
 * @brief Register::setScalarValue
 * @param newValue
 */
void Register::setScalarValue(std::uint64_t newValue) {

	auto from = reinterpret_cast<const char *>(&newValue);
	auto to   = reinterpret_cast<char *>(&value_);
	std::memcpy(to, from, bitSize_ / 8);
}

/**
 * @brief Register::valueAsAddress
 * @return
 */
edb::address_t Register::valueAsAddress() const {
	// This function only makes sense for GPRs
	assert(bitSize_ <= 8 * sizeof(edb::address_t));
	edb::address_t result(0LL);

	auto from = reinterpret_cast<const char *>(&value_);
	auto to   = reinterpret_cast<char *>(&result);
	std::memcpy(to, from, bitSize_ / 8);

	return result;
}
