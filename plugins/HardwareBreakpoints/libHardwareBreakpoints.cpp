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

#include "libHardwareBreakpoints.h"
#include "State.h"

namespace HardwareBreakpoints {

//------------------------------------------------------------------------------
// Name: validateBreakpoint
// Desc:
//------------------------------------------------------------------------------
BreakpointStatus validateBreakpoint(const BreakpointState &bp_state) {

	if(bp_state.enabled) {
		switch(bp_state.type) {
		case 2:
		case 1:
			{
				const edb::address_t address_mask = (1u << bp_state.size) - 1;
				if((bp_state.addr & address_mask) != 0) {
					return AlignmentError;
				}
			}
			break;
		default:
			break;
		}

		if(edb::v1::debuggeeIs32Bit()) {
			if(bp_state.size == 3) {
				return SizeError;
			}
		}
	}

	return Valid;
}

//------------------------------------------------------------------------------
// Name: breakpointState
//------------------------------------------------------------------------------
BreakpointState breakpointState(const State *state, int num) {
	
	Q_ASSERT(num < RegisterCount);
	
	const int N1 = 16 + (num * 4);
	const int N2 = 18 + (num * 4);	
	
	BreakpointState bp_state;
	
	// enabled
	switch(num) {
	case Register1: bp_state.enabled = (state->debug_register(7) & 0x00000001) != 0; break;
	case Register2: bp_state.enabled = (state->debug_register(7) & 0x00000004) != 0; break;
	case Register3: bp_state.enabled = (state->debug_register(7) & 0x00000010) != 0; break;
	case Register4: bp_state.enabled = (state->debug_register(7) & 0x00000040) != 0; break;
	}
	
	// address
	bp_state.addr = state->debug_register(num);
	
	// type
	switch((state->debug_register(7) >> N1) & 0x03) {
	case 0x00:
		bp_state.type = 0;
		break;
	case 0x01:
		bp_state.type = 1;
		break;	
	case 0x03:
		bp_state.type = 2;
		break;	
	default:
		bp_state.type = -1;
		Q_ASSERT(0 && "Internal Error");
	}
	
	// size
	switch((state->debug_register(7) >> N2) & 0x03) {
	case 0x00:
		bp_state.size = 0;
		break;
	case 0x01:
		bp_state.size = 1;
		break;	
	case 0x03:
		bp_state.size = 2;
		break;	
	case 0x02:
		bp_state.size = 3;
	}
	
	return bp_state;
}

//------------------------------------------------------------------------------
// Name: setBreakpointState
// Desc:
//------------------------------------------------------------------------------
void setBreakpointState(State *state, int num, const BreakpointState &bp_state) {

	const int N1 = 16 + (num * 4);
	const int N2 = 18 + (num * 4);


	// default to disabled
	state->set_debug_register(7, (state->debug_register(7) & ~(0x01 << (num * 2))));

	if(bp_state.enabled) {
		// set the address
		state->set_debug_register(num, bp_state.addr);

		// enable this breakpoint
		state->set_debug_register(7, state->debug_register(7) | (0x01 << (num * 2)));

		// setup the type
		switch(bp_state.type) {
		case 2:
			// read/write
			state->set_debug_register(7, (state->debug_register(7) & ~(0x03 << N1)) | (0x03 << N1));
			break;
		case 1:
			// write
			state->set_debug_register(7, (state->debug_register(7) & ~(0x03 << N1)) | (0x01 << N1));
			break;
		case 0:
			// execute
			state->set_debug_register(7, (state->debug_register(7) & ~(0x03 << N1)) | (0x00 << N1));
			break;
		}

		if(bp_state.type != 0) {
			// setup the size
			switch(bp_state.size) {
			case 3:
				// 8 bytes
				Q_ASSERT(edb::v1::debuggeeIs32Bit());
				state->set_debug_register(7, (state->debug_register(7) & ~(0x03 << N2)) | (0x02 << N2));
				break;
			case 2:
				// 4 bytes
				state->set_debug_register(7, (state->debug_register(7) & ~(0x03 << N2)) | (0x03 << N2));
				break;
			case 1:
				// 2 bytes
				state->set_debug_register(7, (state->debug_register(7) & ~(0x03 << N2)) | (0x01 << N2));
				break;
			case 0:
				// 1 byte
				state->set_debug_register(7, (state->debug_register(7) & ~(0x03 << N2)) | (0x00 << N2));
				break;
			}
		} else {
			state->set_debug_register(7, (state->debug_register(7) & ~(0x03 << N2)));
		}
	}
}

}
