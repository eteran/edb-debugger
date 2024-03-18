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
	BasicBlock()                                 = default;
	BasicBlock(const BasicBlock &other)          = default;
	BasicBlock &operator=(const BasicBlock &rhs) = default;
	BasicBlock(BasicBlock &&other)               = default;
	BasicBlock &operator=(BasicBlock &&rhs)      = default;
	~BasicBlock()                                = default;

public:
	void push_back(const instruction_pointer &inst);
	void addReference(edb::address_t refsite, edb::address_t target);

public:
	[[nodiscard]] std::vector<std::pair<edb::address_t, edb::address_t>> references() const;

public:
	[[nodiscard]] reference operator[](size_type pos);
	[[nodiscard]] const_reference operator[](size_type pos) const;

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
	[[nodiscard]] size_type size() const;
	[[nodiscard]] bool empty() const;

public:
	void swap(BasicBlock &other);

public:
	[[nodiscard]] QString toString() const;

public:
	[[nodiscard]] size_type byteSize() const;
	[[nodiscard]] edb::address_t firstAddress() const;
	[[nodiscard]] edb::address_t lastAddress() const;

private:
	std::vector<instruction_pointer> instructions_;
	std::vector<std::pair<edb::address_t, edb::address_t>> references_;
};

#endif
