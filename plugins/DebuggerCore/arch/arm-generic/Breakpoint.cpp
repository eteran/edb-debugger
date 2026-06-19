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

namespace DebuggerCorePlugin {

namespace {
const std::vector<quint8> BreakpointInstructionARM_LE   = {0xf0, 0x01, 0xf0, 0xe7}; // udf #0x10
const std::vector<quint8> BreakpointInstructionThumb_LE = {0x01, 0xde};             // udf #1
// We have to sometimes use a 32-bit Thumb-2 breakpoint. For explanation how to
// correctly use it see GDB's thumb_get_next_pcs_raw function and comments
// around arm_linux_thumb2_le_breakpoint array.
const std::vector<quint8> BreakpointInstructionThumb2_LE = {0xf0, 0xf7, 0x00, 0xa0}; // udf.w #0
// This one generates SIGILL both in ARM32 and Thumb mode. In ARM23 mode it's decoded as UDF 0xDDE0, while
// in Thumb it's a sequence `1: UDF 0xF0; B 1b`, which does stop the process even if it lands in the
// middle (on the second half-word), although the signal still occurs at the first half-word.
const std::vector<quint8> BreakpointInstructionUniversalThumbARM_LE = {0xf0, 0xde, 0xfd, 0xe7};
const std::vector<quint8> BreakpointInstructionThumbBKPT_LE         = {0x00, 0xbe};             // bkpt #0
const std::vector<quint8> BreakpointInstructionARM32BKPT_LE         = {0x70, 0x00, 0x20, 0xe1}; // bkpt #0
}

/**
 * @brief Constructs a new breakpoint at the specified address.
 * @param address The address of the breakpoint.
 */
Breakpoint::Breakpoint(edb::address_t address)
	: address_(address), type_(edb::v1::config().default_breakpoint_type) {

	if (!enable()) {
		throw BreakpointCreationError();
	}
}

/**
 * @brief Returns the list of supported breakpoint types.
 * @return A vector of supported breakpoint types.
 */
auto Breakpoint::supportedTypes() -> std::vector<BreakpointType> {
	std::vector<BreakpointType> types = {
		BreakpointType{Type{TypeId::Automatic}, QObject::tr("Automatic")},
		BreakpointType{Type{TypeId::ARM32}, QObject::tr("Always ARM32 UDF")},
		BreakpointType{Type{TypeId::Thumb2Byte}, QObject::tr("Always Thumb UDF")},
		BreakpointType{Type{TypeId::Thumb4Byte}, QObject::tr("Always Thumb2 UDF.W")},
		BreakpointType{Type{TypeId::UniversalThumbARM32}, QObject::tr("Universal ARM/Thumb UDF(+B .-2)")},
		BreakpointType{Type{TypeId::ARM32BKPT}, QObject::tr("ARM32 BKPT (may be slow)")},
		BreakpointType{Type{TypeId::ThumbBKPT}, QObject::tr("Thumb BKPT (may be slow)")},
	};
	return types;
}

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
 * @brief Destructor for the Breakpoint class.
 */
Breakpoint::~Breakpoint() {
	disable();
}

/**
 * @brief Enables the breakpoint.
 * @return True if the breakpoint was successfully enabled, false otherwise.
 */
bool Breakpoint::enable() {
	if (!enabled()) {
		if (IProcess *process = edb::v1::debugger_core->process()) {
			std::vector<quint8> prev(4);
			prev.resize(process->readBytes(address(), &prev[0], prev.size()));
			if (prev.size()) {
				originalBytes_ = prev;

				const std::vector<quint8> *bpBytes = nullptr;
				switch (TypeId{type_}) {
				case TypeId::Automatic:
					if (edb::v1::debugger_core->cpuMode() == IDebugger::CpuMode::Thumb) {
						bpBytes = &BreakpointInstructionThumb_LE;
					} else {
						bpBytes = &BreakpointInstructionARM_LE;
					}
					break;
				case TypeId::ARM32:
					bpBytes = &BreakpointInstructionARM_LE;
					break;
				case TypeId::Thumb2Byte:
					bpBytes = &BreakpointInstructionThumb_LE;
					break;
				case TypeId::Thumb4Byte:
					bpBytes = &BreakpointInstructionThumb2_LE;
					break;
				case TypeId::UniversalThumbARM32:
					bpBytes = &BreakpointInstructionUniversalThumbARM_LE;
					break;
				case TypeId::ARM32BKPT:
					bpBytes = &BreakpointInstructionARM32BKPT_LE;
					break;
				case TypeId::ThumbBKPT:
					bpBytes = &BreakpointInstructionThumbBKPT_LE;
					break;
				}
				assert(bpBytes);
				assert(originalBytes_.size() >= bpBytes->size());
				originalBytes_.resize(bpBytes->size());

				// FIXME: we don't check whether this breakpoint will overlap any of the existing breakpoints

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
 * @brief Disables the breakpoint.
 * @return True if the breakpoint was successfully disabled, false otherwise.
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
 * @brief Called when the breakpoint is hit.
 */
void Breakpoint::hit() {
	++hitCount_;
}

/**
 * @brief Sets the one-time flag for the breakpoint.
 * @param value The value to set the one-time flag to.
 */
void Breakpoint::setOneTime(bool value) {
	oneTime_ = value;
}

/**
 * @brief Sets the internal flag for the breakpoint.
 * @param value The value to set the internal flag to.
 */
void Breakpoint::setInternal(bool value) {
	internal_ = value;
}

/**
 * @brief Returns the possible rewind sizes for the breakpoint.
 * @return A vector of possible rewind sizes.
 */
std::vector<size_t> Breakpoint::possibleRewindSizes() {
	return {0}; // Even BKPT stops before the instruction, let alone UDF
}

}
