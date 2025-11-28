/*
 * Copyright (C) 2015 Armen Boursalian <aboursalian@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
	[[nodiscard]] StackFrame *operator[](size_t index);
	[[nodiscard]] size_t size() const;
	[[nodiscard]] StackFrame *top();
	[[nodiscard]] StackFrame *bottom();
	void push(StackFrame frame);

private:
	std::deque<StackFrame> stackFrames_;
};

#endif
