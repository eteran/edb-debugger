/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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

public:
	std::pair<BasicBlock, BasicBlock> splitBlock(const instruction_pointer &inst);

private:
	std::vector<instruction_pointer> instructions_;
	std::vector<std::pair<edb::address_t, edb::address_t>> references_;
};

#endif
