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

#include "Breakpoint.h"
#include "IDebugger.h"
#include "IProcess.h"
#include "edb.h"

namespace DebuggerCorePlugin {

namespace {
constexpr std::array<quint8, 4> BreakpointInstructionARM_LE    = { 0xf0, 0x01, 0xf0, 0xe7 }; // udf #0x10
constexpr std::array<quint8, 2> BreakpointInstructionThumb_LE  = { 0x01, 0xde };             // udf #1
// We have to sometimes use a 32-bit Thumb-2 breakpoint. For explanation how to
// correctly use it see GDB's thumb_get_next_pcs_raw function and comments
// around arm_linux_thumb2_le_breakpoint array.
constexpr std::array<quint8, 4> BreakpointInstructionThumb2_LE = { 0xf0, 0xf7, 0x00, 0xa0 }; // udf.w #0
}

//------------------------------------------------------------------------------
// Name: Breakpoint
// Desc: constructor
//------------------------------------------------------------------------------
Breakpoint::Breakpoint(edb::address_t address) : address_(address), hit_count_(0), enabled_(false), one_time_(false), internal_(false) {

	std::fill(original_bytes_.begin(), original_bytes_.end(), 0xff);

	if(!enable()) {
		throw breakpoint_creation_error();
	}
}

//------------------------------------------------------------------------------
// Name: ~Breakpoint
// Desc:
//------------------------------------------------------------------------------
Breakpoint::~Breakpoint() {
	disable();
}

//------------------------------------------------------------------------------
// Name: enable
// Desc:
//------------------------------------------------------------------------------
bool Breakpoint::enable() {
	if(!enabled()) {
		if(IProcess *process = edb::v1::debugger_core->process()) {
			std::array<quint8, 4> prev;
			if(process->read_bytes(address(), &prev[0], prev.size())) {
				original_bytes_ = prev;
				if(process->write_bytes(address(), &BreakpointInstructionARM_LE[0], BreakpointInstructionARM_LE.size())) {
					enabled_ = true;
					return true;
				}
			}
		}
	}
	return false;
}

//------------------------------------------------------------------------------
// Name: disable
// Desc:
//------------------------------------------------------------------------------
bool Breakpoint::disable() {
	if(enabled()) {
		if(IProcess *process = edb::v1::debugger_core->process()) {
			if(process->write_bytes(address(), &original_bytes_[0], original_bytes_.size())) {
				enabled_ = false;
				return true;
			}
		}
	}
	return false;
}

//------------------------------------------------------------------------------
// Name: hit
// Desc:
//------------------------------------------------------------------------------
void Breakpoint::hit() {
	++hit_count_;
}

//------------------------------------------------------------------------------
// Name: set_one_time
// Desc:
//------------------------------------------------------------------------------
void Breakpoint::set_one_time(bool value) {
	one_time_ = value;
}

//------------------------------------------------------------------------------
// Name: set_internal
// Desc:
//------------------------------------------------------------------------------
void Breakpoint::set_internal(bool value) {
	internal_ = value;
}

}
