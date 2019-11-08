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
#include "Configuration.h"
#include "IDebugger.h"
#include "IProcess.h"
#include "edb.h"

namespace DebuggerCorePlugin {

namespace {
const std::vector<uint8_t> BreakpointInstructionINT3  = {0xcc};
const std::vector<uint8_t> BreakpointInstructionINT1  = {0xf1};
const std::vector<uint8_t> BreakpointInstructionHLT   = {0xf4};
const std::vector<uint8_t> BreakpointInstructionCLI   = {0xfa};
const std::vector<uint8_t> BreakpointInstructionSTI   = {0xfb};
const std::vector<uint8_t> BreakpointInstructionINSB  = {0x6c};
const std::vector<uint8_t> BreakpointInstructionINSD  = {0x6d};
const std::vector<uint8_t> BreakpointInstructionOUTSB = {0x6e};
const std::vector<uint8_t> BreakpointInstructionOUTSD = {0x6f};
const std::vector<uint8_t> BreakpointInstructionUD2   = {0x0f, 0x0b};
const std::vector<uint8_t> BreakpointInstructionUD0   = {0x0f, 0xff};
}

/**
 * @brief Breakpoint::Breakpoint
 * @param address
 */
Breakpoint::Breakpoint(edb::address_t address)
	: address_(address), type_(edb::v1::config().default_breakpoint_type) {

	if (!this->enable()) {
		throw BreakpointCreationError();
	}
}

/**
 * @brief Breakpoint::supportedTypes
 * @return
 */
auto Breakpoint::supportedTypes() -> std::vector<BreakpointType> {
	std::vector<BreakpointType> types = {
		BreakpointType{Type{TypeId::Automatic}, tr("Automatic")},
		BreakpointType{Type{TypeId::INT3}, tr("INT3")},
		BreakpointType{Type{TypeId::INT1}, tr("INT1 (ICEBP)")},
		BreakpointType{Type{TypeId::HLT}, tr("HLT")},
		BreakpointType{Type{TypeId::CLI}, tr("CLI")},
		BreakpointType{Type{TypeId::STI}, tr("STI")},
		BreakpointType{Type{TypeId::INSB}, tr("INSB")},
		BreakpointType{Type{TypeId::INSD}, tr("INSD")},
		BreakpointType{Type{TypeId::OUTSB}, tr("OUTSB")},
		BreakpointType{Type{TypeId::OUTSD}, tr("OUTSD")},
		BreakpointType{Type{TypeId::UD2}, tr("UD2 (2-byte)")},
		BreakpointType{Type{TypeId::UD0}, tr("UD0 (2-byte)")},
	};
	return types;
}

/**
 * @brief Breakpoint::setType
 * @param type
 */
void Breakpoint::setType(TypeId type) {
	disable();
	type_ = type;
	if (!enable()) {
		throw BreakpointCreationError();
	}
}

/**
 * @brief Breakpoint::setType
 * @param type
 */
void Breakpoint::setType(IBreakpoint::TypeId type) {
	disable();

	if (Type{type} >= TypeId::TYPE_COUNT) {
		throw BreakpointCreationError();
	}

	setType(type);
}

/**
 * @brief Breakpoint::~Breakpoint
 */
Breakpoint::~Breakpoint() {
	this->disable();
}

/**
 * @brief Breakpoint::enable
 * @return
 */
bool Breakpoint::enable() {
	if (!enabled()) {
		if (IProcess *process = edb::v1::debugger_core->process()) {
			std::vector<uint8_t> prev(2);
			if (process->readBytes(address(), &prev[0], prev.size())) {
				originalBytes_                      = prev;
				const std::vector<uint8_t> *bpBytes = nullptr;

				switch (TypeId{type_}) {
				case TypeId::Automatic:
				case TypeId::INT3:
					bpBytes = &BreakpointInstructionINT3;
					break;
				case TypeId::INT1:
					bpBytes = &BreakpointInstructionINT1;
					break;
				case TypeId::HLT:
					bpBytes = &BreakpointInstructionHLT;
					break;
				case TypeId::CLI:
					bpBytes = &BreakpointInstructionCLI;
					break;
				case TypeId::STI:
					bpBytes = &BreakpointInstructionSTI;
					break;
				case TypeId::INSB:
					bpBytes = &BreakpointInstructionINSB;
					break;
				case TypeId::INSD:
					bpBytes = &BreakpointInstructionINSD;
					break;
				case TypeId::OUTSB:
					bpBytes = &BreakpointInstructionOUTSB;
					break;
				case TypeId::OUTSD:
					bpBytes = &BreakpointInstructionOUTSD;
					break;
				case TypeId::UD2:
					bpBytes = &BreakpointInstructionUD2;
					break;
				case TypeId::UD0:
					bpBytes = &BreakpointInstructionUD0;
					break;
				default:
					return false;
				}

				assert(bpBytes);
				assert(originalBytes_.size() >= bpBytes->size());
				originalBytes_.resize(bpBytes->size());

				if (process->writeBytes(address(), bpBytes->data(), bpBytes->size())) {
					enabled_ = true;
					return true;
				}
			}
		}
	}
	return false;
}

/**
 * @brief Breakpoint::disable
 * @return
 */
bool Breakpoint::disable() {
	if (enabled()) {
		if (IProcess *process = edb::v1::debugger_core->process()) {
			if (process->writeBytes(address(), &originalBytes_[0], originalBytes_.size())) {
				enabled_ = false;
				return true;
			}
		}
	}
	return false;
}

/**
 * @brief Breakpoint::hit
 */
void Breakpoint::hit() {
	++hitCount_;
}

/**
 * @brief Breakpoint::setOneTime
 * @param value
 */
void Breakpoint::setOneTime(bool value) {
	oneTime_ = value;
}

/**
 * @brief Breakpoint::setInternal
 * @param value
 */
void Breakpoint::setInternal(bool value) {
	internal_ = value;
}

/**
 * @brief Breakpoint::possibleRewindSizes
 * @return
 */
std::vector<size_t> Breakpoint::possibleRewindSizes() {
	return {1, 0, 2}; // e.g. int3/int1, cli/sti/hlt/etc., int 0x1/int 0x3
}

}
