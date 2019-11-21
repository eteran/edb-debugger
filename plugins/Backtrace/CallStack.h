/*
Copyright (C) 2015	Armen Boursalian
                    aboursalian@gmail.com

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

#ifndef CALL_STACK_H_20191119_
#define CALL_STACK_H_20191119_

#include "edb.h"
#include <deque>

class CallStack {
public:
	CallStack();
	~CallStack() = default;

public:
	// Struct that holds the caller and return addresses.
	struct StackFrame {
		edb::address_t ret;
		edb::address_t caller;
	};

private:
	void getCallStack();

public:
	StackFrame *operator[](size_t index);
	size_t size() const;
	StackFrame *top();
	StackFrame *bottom();
	void push(StackFrame frame);

private:
	std::deque<StackFrame> stackFrames_;
};

#endif
