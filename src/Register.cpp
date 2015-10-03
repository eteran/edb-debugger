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

//------------------------------------------------------------------------------
// Name: Register
// Desc:
//------------------------------------------------------------------------------
Register::Register() :
	name_("<unknown>"),
	type_(TYPE_INVALID),
	bitSize_(0) {
	util::markMemory(&value_,sizeof(value_));
}

//------------------------------------------------------------------------------
// Name: Register
// Desc:
//------------------------------------------------------------------------------
Register::Register(const Register &other)
	: name_(other.name_),
	  value_(other.value_),
	  type_(other.type_),
	  bitSize_(other.bitSize_) {
}

//------------------------------------------------------------------------------
// Name: operator=
// Desc:
//------------------------------------------------------------------------------
Register &Register::operator=(const Register &rhs) {
	if(this != &rhs) {
		name_         = rhs.name_;
		value_        = rhs.value_;
		type_         = rhs.type_;
		bitSize_      = rhs.bitSize_;
	}
	return *this;
}

//------------------------------------------------------------------------------
// Name: operator==
// Desc:
//------------------------------------------------------------------------------
bool Register::operator==(const Register &rhs) const {
	if(!valid() && !rhs.valid()) {
		return true;
	} else {
		return name_ == rhs.name_ && value_ == rhs.value_ && type_ == rhs.type_ && bitSize_ == rhs.bitSize_;
	}
}

QString Register::toHexString() const {
	if(!valid()) return "<undefined>";
	if(bitSize_%8) return QObject::tr("(Error: bad register length %1 bits)").arg(bitSize_);
	return value_.toHexString().right(bitSize_/4); // TODO: trimming should be moved to valueXX::toHexString()
}

//------------------------------------------------------------------------------
// Name: operator!=
// Desc:
//------------------------------------------------------------------------------
bool Register::operator!=(const Register &rhs) const {
	return !(*this == rhs);
}
