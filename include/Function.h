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
	Function()                      = default;
	Function(const Function &other) = default;
	Function &operator=(const Function &rhs) = default;

public:
	void insert(const BasicBlock &bb);
	void addReference();
	Type type() const;
	void setType(Type t);

public:
	const_reference back() const;
	const_reference front() const;
	reference back();
	reference front();

public:
	const_iterator begin() const;
	const_iterator end() const;
	const_reverse_iterator rbegin() const;
	const_reverse_iterator rend() const;
	iterator begin();
	iterator end();
	reverse_iterator rbegin();
	reverse_iterator rend();

public:
	bool empty() const;
	size_type size() const;

public:
	edb::address_t entryAddress() const;
	edb::address_t endAddress() const;
	edb::address_t lastInstruction() const;
	int referenceCount() const;

public:
	void swap(Function &other);

private:
	int referenceCount_ = 0;
	Type type_          = Standard;
	std::map<edb::address_t, BasicBlock> blocks_;
};

#endif
