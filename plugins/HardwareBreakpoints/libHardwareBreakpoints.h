/*
Copyright (C) 2015 - 2015 Evan Teran
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

#ifndef LIB_HARDWARE_BREAKPOINTS_H_
#define LIB_HARDWARE_BREAKPOINTS_H_

#include "edb.h"

namespace HardwareBreakpoints {

constexpr const int Register1	  = 0;
constexpr const int Register2	  = 1;
constexpr const int Register3	  = 2;
constexpr const int Register4	  = 3;
constexpr const int RegisterCount = 4;

struct BreakpointState {
	bool           enabled;
	edb::address_t addr;
	int            type;
	int            size;
};

enum BreakpointStatus {
	Valid,
	AlignmentError,
	SizeError
};

BreakpointState breakpointState(const State *state, int num);
void setBreakpointState(State *state, int num, const BreakpointState &bp_state);
BreakpointStatus validateBreakpoint(const BreakpointState &bp_state);

}

#endif
