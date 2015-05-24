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

#ifndef CALLSTACK_H
#define CALLSTACK_H

#include "edb.h"

#include <QList>
#include <QtCore/qglobal.h>

class CallStack
{
public:
	CallStack();
	~CallStack();

//Struct that holds the caller and return addresses.
public:
	typedef struct stack_frame_t {
		edb::address_t ret;
		edb::address_t caller;
	} stack_frame;

private:
	void get_call_stack();

public:
	stack_frame *operator [](qint32 index);
	int size();
	stack_frame *top();
	stack_frame *bottom();
	void push(stack_frame frame);

private:
	QList<stack_frame> stack_frames_;
};

#endif // CALLSTACK_H
