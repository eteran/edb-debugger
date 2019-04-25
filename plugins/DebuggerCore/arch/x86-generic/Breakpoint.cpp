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
const std::vector<quint8> BreakpointInstructionINT3  = {0xcc};
const std::vector<quint8> BreakpointInstructionINT1  = {0xf1};
const std::vector<quint8> BreakpointInstructionHLT   = {0xf4};
const std::vector<quint8> BreakpointInstructionCLI   = {0xfa};
const std::vector<quint8> BreakpointInstructionSTI   = {0xfb};
const std::vector<quint8> BreakpointInstructionINSB  = {0x6c};
const std::vector<quint8> BreakpointInstructionINSD  = {0x6d};
const std::vector<quint8> BreakpointInstructionOUTSB = {0x6e};
const std::vector<quint8> BreakpointInstructionOUTSD = {0x6f};
const std::vector<quint8> BreakpointInstructionUD2   = {0x0f, 0x0b};
const std::vector<quint8> BreakpointInstructionUD0   = {0x0f, 0xff};
}

//------------------------------------------------------------------------------
// Name: Breakpoint
// Desc: constructor
//------------------------------------------------------------------------------
Breakpoint::Breakpoint(edb::address_t address) : address_(address), type_(edb::v1::config().default_breakpoint_type) {

    if(!this->enable()) {
		throw breakpoint_creation_error();
	}
}

auto Breakpoint::supported_types() -> std::vector<BreakpointType> {
	std::vector<BreakpointType> types = {
		BreakpointType{Type{TypeId::Automatic},QObject::tr("Automatic")},
		BreakpointType{Type{TypeId::INT3     },QObject::tr("INT3")},
		BreakpointType{Type{TypeId::INT1     },QObject::tr("INT1 (ICEBP)")},
		BreakpointType{Type{TypeId::HLT      },QObject::tr("HLT")},
		BreakpointType{Type{TypeId::CLI      },QObject::tr("CLI")},
		BreakpointType{Type{TypeId::STI      },QObject::tr("STI")},
		BreakpointType{Type{TypeId::INSB     },QObject::tr("INSB")},
		BreakpointType{Type{TypeId::INSD     },QObject::tr("INSD")},
		BreakpointType{Type{TypeId::OUTSB    },QObject::tr("OUTSB")},
		BreakpointType{Type{TypeId::OUTSD    },QObject::tr("OUTSD")},
		BreakpointType{Type{TypeId::UD2      },QObject::tr("UD2 (2-byte)")},
		BreakpointType{Type{TypeId::UD0      },QObject::tr("UD0 (2-byte)")},
	};
	return types;
}


void Breakpoint::set_type(TypeId type) {
	disable();
	type_ = type;
	if(!enable()) {
		throw breakpoint_creation_error();
	}
}

void Breakpoint::set_type(IBreakpoint::TypeId type) {
	disable();

	if(Type{type} >= TypeId::TYPE_COUNT) {
		throw breakpoint_creation_error();
	}

	set_type(type);
}

//------------------------------------------------------------------------------
// Name: ~Breakpoint
// Desc:
//------------------------------------------------------------------------------
Breakpoint::~Breakpoint() {
    this->disable();
}

//------------------------------------------------------------------------------
// Name: enable
// Desc:
//------------------------------------------------------------------------------
bool Breakpoint::enable() {
	if(!enabled()) {
		if(IProcess *process = edb::v1::debugger_core->process()) {
			std::vector<quint8> prev(2);
			if(process->read_bytes(address(), &prev[0], prev.size())) {
				original_bytes_ = prev;
				const std::vector<quint8>* bpBytes = nullptr;

				switch(TypeId{type_}) {
				case TypeId::Automatic:
				case TypeId::INT3:  bpBytes = &BreakpointInstructionINT3;  break;
				case TypeId::INT1:  bpBytes = &BreakpointInstructionINT1;  break;
				case TypeId::HLT:   bpBytes = &BreakpointInstructionHLT;   break;
				case TypeId::CLI:   bpBytes = &BreakpointInstructionCLI;   break;
				case TypeId::STI:   bpBytes = &BreakpointInstructionSTI;   break;
				case TypeId::INSB:  bpBytes = &BreakpointInstructionINSB;  break;
				case TypeId::INSD:  bpBytes = &BreakpointInstructionINSD;  break;
				case TypeId::OUTSB: bpBytes = &BreakpointInstructionOUTSB; break;
				case TypeId::OUTSD: bpBytes = &BreakpointInstructionOUTSD; break;
				case TypeId::UD2:   bpBytes = &BreakpointInstructionUD2;   break;
				case TypeId::UD0:   bpBytes = &BreakpointInstructionUD0;   break;
				default: return false;
				}

				assert(bpBytes);
				assert(original_bytes_.size() >= bpBytes->size());
				original_bytes_.resize(bpBytes->size());

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
	return {1,0,2}; // e.g. int3/int1, cli/sti/hlt/etc., int 0x1/int 0x3
}

}
