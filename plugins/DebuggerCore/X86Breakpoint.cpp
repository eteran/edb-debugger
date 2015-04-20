/*
Copyright (C) 2006 - 2014 Evan Teran
                          eteran@alum.rit.edu

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

#include "X86Breakpoint.h"
#include "IDebuggerCore.h"
#include "edb.h"

namespace DebuggerCore {

namespace {
const quint8 BreakpointInstruction = 0xcc;
}

//------------------------------------------------------------------------------
// Name: X86Breakpoint
// Desc: constructor
//------------------------------------------------------------------------------
X86Breakpoint::X86Breakpoint(edb::address_t address) : address_(address), hit_count_(0), enabled_(false), one_time_(false), internal_(false) {
	enable();
}

//------------------------------------------------------------------------------
// Name: ~X86Breakpoint
// Desc:
//------------------------------------------------------------------------------
X86Breakpoint::~X86Breakpoint() {
	disable();
}

//------------------------------------------------------------------------------
// Name: enable
// Desc:
//------------------------------------------------------------------------------
bool X86Breakpoint::enable() {
	if(!enabled()) {
		quint8 prev[1];
		if(edb::v1::debugger_core->read_bytes(address(), prev, 1)) {
			if(edb::v1::debugger_core->write_bytes(address(), &BreakpointInstruction, 1)) {
				original_byte_ = prev[0];
				enabled_ = true;
				return true;
			}
		}
	}
	return false;
}

//------------------------------------------------------------------------------
// Name: disable
// Desc:
//------------------------------------------------------------------------------
bool X86Breakpoint::disable() {
	if(enabled()) {
		if(edb::v1::debugger_core->write_bytes(address(), &original_byte_, 1)) {
			enabled_ = false;
			return true;
		}
	}
	return false;
}

//------------------------------------------------------------------------------
// Name: hit
// Desc:
//------------------------------------------------------------------------------
void X86Breakpoint::hit() {
	++hit_count_;
}

//------------------------------------------------------------------------------
// Name: set_one_time
// Desc:
//------------------------------------------------------------------------------
void X86Breakpoint::set_one_time(bool value) {
	one_time_ = value;
}

//------------------------------------------------------------------------------
// Name: set_internal
// Desc:
//------------------------------------------------------------------------------
void X86Breakpoint::set_internal(bool value) {
	internal_ = value;
}

}
