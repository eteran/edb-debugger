/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "Register.h"
#include "Util.h"

/**
 * @brief Constructs a new Register object.
 */
Register::Register() {
	util::mark_memory(&value_, sizeof(value_));
}

/**
 * @brief Compares two Register objects for equality.
 * @param rhs The right-hand side register to compare.
 * @return True if the registers are equal, false otherwise.
 */
bool Register::operator==(const Register &rhs) const {
	if (!valid() && !rhs.valid()) {
		return true;
	}

	return name_ == rhs.name_ && value_ == rhs.value_ && type_ == rhs.type_ && bitSize_ == rhs.bitSize_;
}

/**
 * @brief Formats the register value as a hexadecimal string.
 * @return The formatted register value.
 */
QString Register::toHexString() const {
	if (!valid()) {
		return tr("<undefined>");
	}

	if (bitSize_ % 8) {
		return tr("(Error: bad register length %1 bits)").arg(bitSize_);
	}

	return value_.toHexString().right(static_cast<int>(bitSize_ / 4)); // TODO: trimming should be moved to valueXX::toHexString()
}

/**
 * @brief Compares two Register objects for inequality.
 * @param rhs The right-hand side register to compare.
 * @return True if the registers are not equal, false otherwise.
 */
bool Register::operator!=(const Register &rhs) const {
	return !(*this == rhs);
}

/**
 * @brief Sets the scalar value of the register.
 * @param newValue The new scalar value to set.
 */
void Register::setScalarValue(std::uint64_t newValue) {

	auto from = reinterpret_cast<const char *>(&newValue);
	auto to   = reinterpret_cast<char *>(&value_);
	std::memcpy(to, from, bitSize_ / 8);
}

/**
 * @brief Returns the value of the register as an address.
 * @return The value of the register as an address.
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
