/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "Breakpoint.h"
#include "Configuration.h"
#include "IDebugger.h"
#include "IProcess.h"
#include "edb.h"
#include <cassert>

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
 * @brief Constructs a breakpoint at the given address, writing the trap instruction immediately.
 *
 * @param address
 */
Breakpoint::Breakpoint(edb::address_t address)
	: address_(address), type_(edb::v1::config().default_breakpoint_type) {

	if (!this->enable()) {
		throw BreakpointCreationError();
	}

	module_ = edb::v2::module_for_address(address);
}

/**
 * @brief Returns the list of all software breakpoint instruction types supported on x86/x86-64.
 *
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
 * @brief Disables the current breakpoint, changes its instruction type, and re-enables it.
 *
 * @param type
 */
void Breakpoint::setType(IBreakpoint::TypeId type) {
	disable();

	if (Type{type} >= TypeId::TYPE_COUNT) {
		throw BreakpointCreationError();
	}

	type_ = type;

	if (!enable()) {
		throw BreakpointCreationError();
	}
}

/**
 * @brief Destroys the breakpoint and restores the original instruction bytes.
 */
Breakpoint::~Breakpoint() {
	disable();
}

/**
 * @brief Writes the breakpoint instruction bytes to the target process, saving the original bytes.
 *
 * @return
 */
bool Breakpoint::enable() {
	if (!enabled()) {
		if (IProcess *process = edb::v1::debugger_core->process()) {
			std::vector<uint8_t> prev(2);
			if (process->readBytes(address(), prev.data(), prev.size())) {
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
 * @brief Restores the original instruction bytes in the target process to remove the breakpoint.
 *
 * @return
 */
bool Breakpoint::disable() {
	if (enabled()) {
		if (IProcess *process = edb::v1::debugger_core->process()) {
			if (process->writeBytes(address(), originalBytes_.data(), originalBytes_.size())) {
				enabled_ = false;
				return true;
			}
		}
	}
	return false;
}

/**
 * @brief Increments the breakpoint hit count.
 */
void Breakpoint::hit() {
	++hitCount_;
}

/**
 * @brief Sets whether the breakpoint should automatically remove itself after being hit once.
 *
 * @param value
 */
void Breakpoint::setOneTime(bool value) {
	oneTime_ = value;
}

/**
 * @brief Sets whether the breakpoint is internal (not shown to the user in the UI).
 *
 * @param value
 */
void Breakpoint::setInternal(bool value) {
	internal_ = value;
}

/**
 * @brief Returns the possible instruction sizes (in bytes) to rewind the PC when a breakpoint is triggered.
 *
 * @return
 */
std::vector<size_t> Breakpoint::possibleRewindSizes() {
	return {1, 0, 2}; // e.g. int3/int1, cli/sti/hlt/etc., int 0x1/int 0x3
}

}
