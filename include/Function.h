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

#ifndef FUNCTION_20130830_H_
#define FUNCTION_20130830_H_

#include "API.h"
#include "Types.h"
#include "BasicBlock.h"
#include <QMap>

class EDB_EXPORT Function {
public:
	enum Type {
		FUNCTION_STANDARD,
		FUNCTION_THUNK
	};

public:
	typedef size_t                                             size_type;
	typedef BasicBlock                                         value_type;
	typedef BasicBlock                                        &reference;
	typedef const BasicBlock                                  &const_reference;
	typedef QMap<edb::address_t, BasicBlock>::iterator         iterator;
	typedef QMap<edb::address_t, BasicBlock>::const_iterator   const_iterator;
	typedef std::reverse_iterator<iterator>                    reverse_iterator;
	typedef std::reverse_iterator<const_iterator>              const_reverse_iterator;

public:
	Function();
	explicit Function(edb::address_t address);
	Function(const Function &other);
	Function &operator=(const Function &rhs);
	
public:
	void insert(const BasicBlock &bb);
	void add_reference();
	Type type() const;
	void set_type(Type t);

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
	edb::address_t entry_address() const;
	edb::address_t end_address() const;
	edb::address_t last_instruction() const;
	int reference_count() const;
	
public:
	void swap(Function &other);
	
private:
	edb::address_t                   address_;
	int                              reference_count_;
	Type                             type_;
	QMap<edb::address_t, BasicBlock> blocks_;
};

#endif
