/*
Copyright (C) 2006 - 2014 Evan Teran
                          eteran@alum.rit.edu

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

//------------------------------------------------------------------------------
// Name: Register
// Desc:
//------------------------------------------------------------------------------
Register::Register() : value_(0), type_(TYPE_INVALID) {
}

//------------------------------------------------------------------------------
// Name: Register
// Desc:
//------------------------------------------------------------------------------
Register::Register(const QString &name, edb::reg_t value, Type type) : name_(name), value_(value), type_(type) {
}

//------------------------------------------------------------------------------
// Name: Register
// Desc:
//------------------------------------------------------------------------------
Register::Register(const Register &other) : name_(other.name_), value_(other.value_), type_(other.type_) {
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
		return name_ == rhs.name_ && value_ == rhs.value_ && type_ == rhs.type_;
	}
}

//------------------------------------------------------------------------------
// Name: operator!=
// Desc:
//------------------------------------------------------------------------------
bool Register::operator!=(const Register &rhs) const {
	return !(*this == rhs);
}
