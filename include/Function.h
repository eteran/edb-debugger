/*
Copyright (C) 2006 - 2023 Evan Teran
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

#ifndef FUNCTION_H_20130830_
#define FUNCTION_H_20130830_

#include "API.h"
#include "BasicBlock.h"
#include "Types.h"
#include <map>

class EDB_EXPORT Function {
public:
	enum Type {
		Standard,
		Thunk
	};

public:
	using size_type              = size_t;
	using value_type             = BasicBlock;
	using reference              = BasicBlock &;
	using const_reference        = const BasicBlock &;
	using iterator               = std::map<edb::address_t, BasicBlock>::iterator;
	using const_iterator         = std::map<edb::address_t, BasicBlock>::const_iterator;
	using reverse_iterator       = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

public:
	Function()                               = default;
	Function(const Function &other)          = default;
	Function &operator=(const Function &rhs) = default;

public:
	void insert(const BasicBlock &bb);
	void addReference();
	[[nodiscard]] Type type() const;
	void setType(Type t);

public:
	[[nodiscard]] const_reference back() const;
	[[nodiscard]] const_reference front() const;
	[[nodiscard]] reference back();
	[[nodiscard]] reference front();

public:
	[[nodiscard]] const_iterator begin() const;
	[[nodiscard]] const_iterator end() const;
	[[nodiscard]] const_reverse_iterator rbegin() const;
	[[nodiscard]] const_reverse_iterator rend() const;
	[[nodiscard]] iterator begin();
	[[nodiscard]] iterator end();
	[[nodiscard]] reverse_iterator rbegin();
	[[nodiscard]] reverse_iterator rend();

public:
	[[nodiscard]] bool empty() const;
	[[nodiscard]] size_type size() const;

public:
	[[nodiscard]] edb::address_t entryAddress() const;
	[[nodiscard]] edb::address_t endAddress() const;
	[[nodiscard]] edb::address_t lastInstruction() const;
	[[nodiscard]] int referenceCount() const;

public:
	void swap(Function &other);

private:
	int referenceCount_ = 0;
	Type type_          = Standard;
	std::map<edb::address_t, BasicBlock> blocks_;
};

#endif
