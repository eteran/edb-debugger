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

#ifndef BASIC_BLOCK_20130830_H_
#define BASIC_BLOCK_20130830_H_

#include "API.h"
#include "Types.h"

#include <QVector>
#include <memory>
#include <iterator>

class QString;

typedef std::shared_ptr<edb::Instruction> instruction_pointer;

class EDB_EXPORT BasicBlock {
public:
	typedef std::shared_ptr<BasicBlock>                   pointer;

public:
	typedef size_t                                       size_type;
	typedef instruction_pointer                          value_type;
	typedef instruction_pointer                         &reference;
	typedef const instruction_pointer                   &const_reference;
	typedef QVector<instruction_pointer>::iterator       iterator;
	typedef QVector<instruction_pointer>::const_iterator const_iterator;
	typedef std::reverse_iterator<iterator>              reverse_iterator;
	typedef std::reverse_iterator<const_iterator>        const_reverse_iterator;
	
public:
	BasicBlock();
	BasicBlock(const BasicBlock &other);
	BasicBlock &operator=(const BasicBlock &rhs);
	~BasicBlock();
	
public:
	void push_back(const instruction_pointer &inst);
	
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
	size_type byte_size() const;
	edb::address_t first_address() const;
	edb::address_t last_address() const;

private:
	QVector<instruction_pointer> instructions_;
};

#endif
