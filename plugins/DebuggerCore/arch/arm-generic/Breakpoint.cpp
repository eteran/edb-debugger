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
#include "Configuration.h"

namespace DebuggerCorePlugin {

namespace {
const std::vector<quint8> BreakpointInstructionARM_LE    = { 0xf0, 0x01, 0xf0, 0xe7 }; // udf #0x10
const std::vector<quint8> BreakpointInstructionThumb_LE  = { 0x01, 0xde };             // udf #1
// We have to sometimes use a 32-bit Thumb-2 breakpoint. For explanation how to
// correctly use it see GDB's thumb_get_next_pcs_raw function and comments
// around arm_linux_thumb2_le_breakpoint array.
const std::vector<quint8> BreakpointInstructionThumb2_LE = { 0xf0, 0xf7, 0x00, 0xa0 }; // udf.w #0
// This one generates SIGILL both in ARM32 and Thumb mode. In ARM23 mode it's decoded as UDF 0xDDE0, while
// in Thumb it's a sequence `1: UDF 0xF0; B 1b`, which does stop the process even if it lands in the
// middle (on the second half-word), although the signal still occurs at the first half-word.
const std::vector<quint8> BreakpointInstructionUniversalThumbARM_LE = { 0xf0, 0xde, 0xfd, 0xe7 };
const std::vector<quint8> BreakpointInstructionThumbBKPT_LE  = { 0x00, 0xbe }; // bkpt #0
const std::vector<quint8> BreakpointInstructionARM32BKPT_LE  = { 0x70, 0x00, 0x20, 0xe1 }; // bkpt #0
}

//------------------------------------------------------------------------------
// Name: Breakpoint
// Desc: constructor
//------------------------------------------------------------------------------
Breakpoint::Breakpoint(edb::address_t address) : address_(address), hit_count_(0), enabled_(false), one_time_(false), internal_(false), type_(edb::v1::config().default_breakpoint_type) {

	if(!enable()) {
		throw breakpoint_creation_error();
	}
}

auto Breakpoint::supported_types() -> std::vector<BreakpointType> {
	std::vector<BreakpointType> types = {
		BreakpointType{Type{TypeId::Automatic          },QObject::tr("Automatic")},
		BreakpointType{Type{TypeId::ARM32              },QObject::tr("Always ARM32 UDF")},
		BreakpointType{Type{TypeId::Thumb2Byte         },QObject::tr("Always Thumb UDF")},
		BreakpointType{Type{TypeId::Thumb4Byte         },QObject::tr("Always Thumb2 UDF.W")},
		BreakpointType{Type{TypeId::UniversalThumbARM32},QObject::tr("Universal ARM/Thumb UDF(+B .-2)")},
		BreakpointType{Type{TypeId::ARM32BKPT          },QObject::tr("ARM32 BKPT (may be slow)")},
		BreakpointType{Type{TypeId::ThumbBKPT          },QObject::tr("Thumb BKPT (may be slow)")},
	};
	return types;
}

void Breakpoint::set_type(TypeId type) {
	disable();
	type_=type;
	if(!enable()) {
		throw breakpoint_creation_error();
	}
}

void Breakpoint::set_type(IBreakpoint::TypeId type) {
	disable();
	if(Type{type}>=TypeId::TYPE_COUNT)
		throw breakpoint_creation_error();
	set_type(type);
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
			std::vector<quint8> prev(4);
			prev.resize(process->read_bytes(address(), &prev[0], prev.size()));
			if(prev.size()) {
				original_bytes_ = prev;

				const std::vector<quint8>* bpBytes=nullptr;
				switch(TypeId{type_})
				{
				case TypeId::Automatic:
					if(edb::v1::debugger_core->cpu_mode()==IDebugger::CPUMode::Thumb) {
						bpBytes=&BreakpointInstructionThumb_LE;
					} else {
						bpBytes=&BreakpointInstructionARM_LE;
					}
					break;
				case TypeId::ARM32:               bpBytes=&BreakpointInstructionARM_LE; break;
				case TypeId::Thumb2Byte:          bpBytes=&BreakpointInstructionThumb_LE; break;
				case TypeId::Thumb4Byte:          bpBytes=&BreakpointInstructionThumb2_LE; break;
				case TypeId::UniversalThumbARM32: bpBytes=&BreakpointInstructionUniversalThumbARM_LE; break;
				case TypeId::ARM32BKPT:           bpBytes=&BreakpointInstructionARM32BKPT_LE; break;
				case TypeId::ThumbBKPT:           bpBytes=&BreakpointInstructionThumbBKPT_LE; break;
				}
				assert(bpBytes);
				assert(original_bytes_.size() >= bpBytes->size());
				original_bytes_.resize(bpBytes->size());

				// FIXME: we don't check whether this breakpoint will overlap any of the existing breakpoints

				if(process->write_bytes(address(), bpBytes->data(), bpBytes->size())) {
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

std::vector<size_t> Breakpoint::possible_rewind_sizes() {
	return {0}; // Even BKPT stops before the instruction, let alone UDF
}

}
