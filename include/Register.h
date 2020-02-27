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

#ifndef REGISTER_H_20100329_
#define REGISTER_H_20100329_

#include "API.h"
#include "Types.h"
#include "Util.h"
#include <QCoreApplication>
#include <QString>
#include <cassert>
#include <cstring>
#include <type_traits>

class EDB_EXPORT Register {
	Q_DECLARE_TR_FUNCTIONS(Register)

public:
	enum Type {
		// groups, these catagories should remain as portable as possible
		TYPE_INVALID = 0x0000,
		TYPE_GPR     = 0x0001,
		TYPE_IP      = 0x0002,
		TYPE_SEG     = 0x0004,
		TYPE_COND    = 0x0008,
		TYPE_FPU     = 0x0010,
		TYPE_SIMD    = 0x0020,
	};

	// Expand when AVX-512 instructions and state are supported
	using StoredType = edb::value256;

public:
	Register();
	Register(const Register &other) = default;
	Register &operator=(const Register &rhs) = default;

public:
	bool operator==(const Register &rhs) const;
	bool operator!=(const Register &rhs) const;

public:
	bool valid() const { return type_ != TYPE_INVALID; }
	explicit operator bool() const { return valid(); }

	Type type() const { return type_; }
	QString name() const { return name_; }
	std::size_t bitSize() const { return bitSize_; }
	const char *rawData() const { return reinterpret_cast<const char *>(&value_); }

	template <class T>
	T value() const { return T(value_); }

	// Return the value, zero-extended to address_t to be usable in address calculations
	edb::address_t valueAsAddress() const;

	uint64_t valueAsInteger() const {
		return valueAsAddress().toUint();
	}

	int64_t valueAsSignedInteger() const {
		uint64_t result = valueAsInteger();
		// If MSB is set, sign extend the result
		if (result & (1ll << (bitSize_ - 1))) {
			result = -1ll;
			std::memcpy(&result, &value_, bitSize_ / 8);
		}
		return result;
	}

	void setScalarValue(std::uint64_t newValue);

	template <typename T>
	void setValueFrom(const T &source) {
		assert(bitSize_ <= 8 * sizeof(source));

		// NOTE(eteran): used to avoid warnings from GCC >= 8.2
		auto from = reinterpret_cast<const char *>(&source);
		auto to   = reinterpret_cast<char *>(&value_);

		std::memcpy(to, from, bitSize_ / 8);
	}

	QString toHexString() const;

private:
	QString name_        = tr("<unknown>");
	StoredType value_    = {};
	Type type_           = TYPE_INVALID;
	std::size_t bitSize_ = 0;

	template <std::size_t bitSize, typename T>
	friend Register make_Register(const QString &name, T value, Register::Type type);
};

template <std::size_t BitSize, typename T>
Register make_Register(const QString &name, T value, Register::Type type) {

	constexpr std::size_t bitSize = (BitSize ? BitSize : 8 * sizeof(T));
	static_assert(BitSize % 8 == 0, "Strange bit size");

	Register reg;
	reg.name_    = name;
	reg.type_    = type;
	reg.bitSize_ = bitSize;

	constexpr std::size_t size = bitSize / 8;
	static_assert(size <= sizeof(T), "ValueType appears smaller than size specified");
	util::mark_memory(&reg.value_, sizeof(reg.value_));

	// NOTE(eteran): used to avoid warnings from GCC >= 8.2
	auto from = reinterpret_cast<const char *>(&value);
	auto to   = reinterpret_cast<char *>(&reg.value_);

	std::memcpy(to, from, size);

	return reg;
}

template <typename T>
Register make_Register(const QString &name, T value, Register::Type type) {
	return make_Register<0>(name, value, type);
}

#endif
