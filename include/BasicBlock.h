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

#ifndef BASIC_BLOCK_H_20130830_
#define BASIC_BLOCK_H_20130830_

#include "API.h"
#include "Types.h"
#include <iterator>
#include <memory>
#include <vector>

class QString;

using instruction_pointer = std::shared_ptr<edb::Instruction>;

class EDB_EXPORT BasicBlock {
public:
	using size_type              = size_t;
	using value_type             = instruction_pointer;
	using reference              = instruction_pointer &;
	using const_reference        = const instruction_pointer &;
	using iterator               = std::vector<instruction_pointer>::iterator;
	using const_iterator         = std::vector<instruction_pointer>::const_iterator;
	using reverse_iterator       = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

public:
	BasicBlock()                        = default;
	BasicBlock(const BasicBlock &other) = default;
	BasicBlock &operator=(const BasicBlock &rhs) = default;
	BasicBlock(BasicBlock &&other)               = default;
	BasicBlock &operator=(BasicBlock &&rhs) = default;
	~BasicBlock()                           = default;

public:
	void push_back(const instruction_pointer &inst);
	void addReference(edb::address_t refsite, edb::address_t target);

public:
	std::vector<std::pair<edb::address_t, edb::address_t>> references() const;

public:
	reference operator[](size_type pos);
	const_reference operator[](size_type pos) const;

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
	size_type size() const;
	bool empty() const;

public:
	void swap(BasicBlock &other);

public:
	QString toString() const;

public:
	size_type byteSize() const;
	edb::address_t firstAddress() const;
	edb::address_t lastAddress() const;

private:
	std::vector<instruction_pointer> instructions_;
	std::vector<std::pair<edb::address_t, edb::address_t>> references_;
};

#endif
