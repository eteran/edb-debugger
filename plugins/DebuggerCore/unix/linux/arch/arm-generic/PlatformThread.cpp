/*
Copyright (C) 2015 - 2015 Evan Teran
                          evan.teran@gmail.com
Copyright (C) 2017 Ruslan Kabatsayev
                   b7.10110111@gmail.com

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

#include "PlatformThread.h"
#include "ArchProcessor.h"
#include "Breakpoint.h"
#include "DebuggerCore.h"
#include "IProcess.h"
#include "Instruction.h"
#include "PlatformCommon.h"
#include "PlatformState.h"
#include "State.h"
#include "Types.h"
#include <QtDebug>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* or _BSD_SOURCE or _SVID_SOURCE */
#endif

#include <elf.h>
#include <linux/uio.h>
#include <sys/ptrace.h>
#include <sys/user.h>

// doesn't always seem to be defined in the headers
#ifndef PTRACE_GET_THREAD_AREA
#define PTRACE_GET_THREAD_AREA static_cast<__ptrace_request>(22)
#endif

#ifndef PTRACE_GETSIGINFO
#define PTRACE_GETSIGINFO static_cast<__ptrace_request>(0x4202)
#endif

#ifndef PTRACE_GETREGSET
#define PTRACE_GETREGSET static_cast<__ptrace_request>(0x4204)
#endif

#ifndef PTRACE_SETREGSET
#define PTRACE_SETREGSET static_cast<__ptrace_request>(0x4205)
#endif

#ifndef PTRACE_GETWMMXREGS
#define PTRACE_GETWMMXREGS static_cast<__ptrace_request>(18)
#define PTRACE_SETWMMXREGS static_cast<__ptrace_request>(19)
#endif

#ifndef PTRACE_GETVFPREGS
#define PTRACE_GETVFPREGS static_cast<__ptrace_request>(27)
#define PTRACE_SETVFPREGS static_cast<__ptrace_request>(28)
#endif

#ifndef PTRACE_GETHBPREGS
#define PTRACE_GETHBPREGS static_cast<__ptrace_request>(29)
#define PTRACE_SETHBPREGS static_cast<__ptrace_request>(30)
#endif

namespace DebuggerCorePlugin {

/**
 * @brief PlatformThread::fillStateFromPrStatus
 * @param state
 * @return
 */
bool PlatformThread::fillStateFromPrStatus(PlatformState *state) {

	return false;
}

/**
 * @brief PlatformThread::fillStateFromSimpleRegs
 * @param state
 * @return
 */
bool PlatformThread::fillStateFromSimpleRegs(PlatformState *state) {

	user_regs regs;
	if (ptrace(PTRACE_GETREGS, tid_, 0, &regs) != -1) {

		state->fillFrom(regs);
		return true;
	} else {
		perror("PTRACE_GETREGS failed");
		return false;
	}
}

/**
 * @brief PlatformThread::fillStateFromVFPRegs
 * @param state
 * @return
 */
bool PlatformThread::fillStateFromVFPRegs(PlatformState *state) {

	user_vfp fpr;
	if (ptrace(PTRACE_GETVFPREGS, tid_, 0, &fpr) != -1) {
		for (unsigned i = 0; i < sizeof fpr.fpregs / sizeof *fpr.fpregs; ++i)
			state->fillFrom(fpr);
		return true;
	} else {
		perror("PTRACE_GETVFPREGS failed");
		return false;
	}
}

/**
 * @brief PlatformThread::getState
 * @param state
 */
void PlatformThread::getState(State *state) {
	// TODO: assert that we are paused

	core_->detectCpuMode();

	if (auto state_impl = static_cast<PlatformState *>(state->impl_.get())) {

		fillStateFromSimpleRegs(state_impl);
		fillStateFromVFPRegs(state_impl);
	}
}

/**
 * @brief PlatformThread::setState
 * @param state
 */
void PlatformThread::setState(const State &state) {

	// TODO: assert that we are paused

	if (auto state_impl = static_cast<PlatformState *>(state.impl_.get())) {

		user_regs regs;
		state_impl->fillStruct(regs);
		if (ptrace(PTRACE_SETREGS, tid_, 0, &regs) == -1) {
			perror("PTRACE_SETREGS failed");
		}

		user_vfp fpr;
		state_impl->fillStruct(fpr);
		if (ptrace(PTRACE_SETVFPREGS, tid_, 0, &fpr) == -1) {
			perror("PTRACE_SETVFPREGS failed");
		}
	}
}

/**
 * @brief PlatformThread::getDebugRegister
 * @param n
 * @return
 */
unsigned long PlatformThread::getDebugRegister(std::size_t n) {
	return 0;
}

/**
 * @brief PlatformThread::setDebugRegister
 * @param n
 * @param value
 * @return
 */
long PlatformThread::setDebugRegister(std::size_t n, long value) {
	return 0;
}

/**
 * @brief PlatformThread::instructionPointer
 * @return
 */
edb::address_t PlatformThread::instructionPointer() const {
	return 0;
}

/**
 * @brief PlatformThread::doStep
 * @param tid
 * @param status
 * @return
 */
Status PlatformThread::doStep(const edb::tid_t tid, const long status) {

	constexpr auto AddressSize = 4; // The code here is ARM32-specific anyway...

	State state;
	getState(&state);
	if (state.empty()) return Status(tr("failed to get thread state."));
	const auto pc    = state.instructionPointer();
	const auto flags = state.flags();
	enum {
		CPSR_Tbit = 1 << 5,

		CPSR_ITbits72 = 1 << 10,
		CPSR_ITmask72 = 0xfc00,

		CPSR_Jbit = 1 << 24,

		CPSR_ITbits10 = 1 << 25,
		CPSR_ITmask10 = 0x06000000,
	};
	if (flags & CPSR_Jbit)
		return Status(tr("EDB doesn't yet support single-stepping in Jazelle state."));
	if (flags & CPSR_Tbit && flags & (CPSR_ITmask10 | CPSR_ITmask72))
		return Status(tr("EDB doesn't yet support single-stepping inside Thumb-2 IT-block."));
	quint8 buffer[4];
	if (const int size = edb::v1::get_instruction_bytes(pc, buffer)) {
		if (const auto insn = edb::Instruction(buffer, buffer + size, pc)) {

			const auto op                = insn.operation();
			edb::address_t addrAfterInsn = pc + insn.byteSize();

			auto targetMode = core_->cpuMode();
			if (modifies_pc(insn) && edb::v1::arch_processor().isExecuted(insn, state)) {
				if (op == ARM_INS_BXJ)
					return Status(tr("EDB doesn't yet support single-stepping into Jazelle state."));

				const auto opCount = insn.operandCount();
				if (opCount == 0)
					return Status(tr("instruction %1 isn't supported yet.").arg(insn.mnemonic().c_str()));

				switch (op) {
				case ARM_INS_LDR: {
					const auto destOperand = insn.operand(0);
					if (!is_register(destOperand) || destOperand->reg != ARM_REG_PC)
						return Status(tr("instruction %1 with non-PC destination isn't supported yet.").arg(insn.mnemonic().c_str()));
					const auto srcOperand = insn.operand(1);
					if (!is_expression(srcOperand))
						return Status(tr("unexpected type of second operand of LDR instruction."));
					const auto effAddrR = edb::v1::arch_processor().getEffectiveAddress(insn, srcOperand, state);
					if (!effAddrR) return Status(effAddrR.error());

					const auto effAddr = effAddrR.value();
					if (process_->readBytes(effAddr, &addrAfterInsn, AddressSize) != AddressSize)
						return Status(tr("failed to read memory referred to by LDR operand (address %1).").arg(effAddr.toPointerString()));

					// FIXME: for ARMv5 or below (without "T" in the name) bits [1:0] are simply ignored, without any mode change
					if (addrAfterInsn & 1)
						targetMode = IDebugger::CpuMode::Thumb;
					else
						targetMode = IDebugger::CpuMode::ARM32;
					switch (edb::v1::debugger_core->cpuMode()) {
					case IDebugger::CpuMode::Thumb:
						addrAfterInsn &= -2;
						break;
					case IDebugger::CpuMode::ARM32:
						addrAfterInsn &= -4;
						break;
					default:
						return Status(tr("single-stepping LDR instruction in modes other than ARM or Thumb is not supported yet."));
					}
					break;
				}
				case ARM_INS_POP: {
					int i = 0;
					for (; i < opCount; ++i) {
						const auto operand = insn.operand(i);
						if (is_register(operand) && operand->reg == ARM_REG_PC) {
#if CS_API_MAJOR >= 4
							assert(operand->access == CS_AC_WRITE);
#endif
							const auto sp = state.gpRegister(PlatformState::GPR::SP);
							if (!sp) return Status(tr("failed to get value of SP register"));
							if (process_->readBytes(sp.valueAsAddress() + AddressSize * i, &addrAfterInsn, AddressSize) != AddressSize)
								return Status(tr("failed to read thread stack"));
							break;
						}
					}
					if (i == opCount)
						return Status(tr("internal EDB error: failed to locate PC in the instruction operand list"));
					break;
				}
				case ARM_INS_BX:
				case ARM_INS_BLX:
				case ARM_INS_B:
				case ARM_INS_BL: {
					if (opCount != 1)
						return Status(tr("unexpected form of instruction %1 with %2 operands.").arg(insn.mnemonic().c_str()).arg(opCount));
					const auto &operand = insn.operand(0);
					assert(operand);
					if (is_immediate(operand)) {
						addrAfterInsn = edb::address_t(util::to_unsigned(operand->imm));
						if (op == ARM_INS_BX || op == ARM_INS_BLX) {
							if (targetMode == IDebugger::CpuMode::ARM32)
								targetMode = IDebugger::CpuMode::Thumb;
							else
								targetMode = IDebugger::CpuMode::ARM32;
						}
						break;
					} else if (is_register(operand)) {
						if (operand->reg == ARM_REG_PC && (op == ARM_INS_BX || op == ARM_INS_BLX))
							return Status(tr("unpredictable instruction"));
						// This may happen only with BX or BLX: B and BL require an immediate operand
						const auto result = edb::v1::arch_processor().getEffectiveAddress(insn, operand, state);
						if (!result) return Status(result.error());
						addrAfterInsn = result.value();
						if (addrAfterInsn & 1)
							targetMode = IDebugger::CpuMode::Thumb;
						else
							targetMode = IDebugger::CpuMode::ARM32;
						addrAfterInsn &= ~1;
						if (addrAfterInsn & 0x3 && targetMode != IDebugger::CpuMode::Thumb)
							return Status(tr("won't try to set breakpoint at unaligned address"));
						break;
					}
					return Status(tr("bad operand for %1 instruction.").arg(insn.mnemonic().c_str()));
				}
				default:
					return Status(tr("instruction %1 modifies PC, but isn't a branch instruction known to EDB's single-stepper.").arg(insn.mnemonic().c_str()));
				}
			}

			if (singleStepBreakpoint)
				return Status(tr("internal EDB error: single-step breakpoint still present"));
			if (const auto oldBP = core_->findBreakpoint(addrAfterInsn)) {
				// TODO: EDB should support overlapping breakpoints
				if (!oldBP->enabled())
					return Status(tr("a disabled breakpoint is present at address %1, can't set one for single step.").arg(addrAfterInsn.toPointerString()));
			} else {
				singleStepBreakpoint = core_->addBreakpoint(addrAfterInsn);
				if (!singleStepBreakpoint)
					return Status(tr("failed to set breakpoint at address %1.").arg(addrAfterInsn.toPointerString()));
				const auto bp = std::static_pointer_cast<Breakpoint>(singleStepBreakpoint);
				if (targetMode != core_->cpuMode()) {
					switch (targetMode) {
					case IDebugger::CpuMode::ARM32:
						bp->setType(Breakpoint::TypeId::ARM32);
						break;
					case IDebugger::CpuMode::Thumb:
						bp->setType(Breakpoint::TypeId::Thumb2Byte);
						break;
					}
				}
				singleStepBreakpoint->setOneTime(true); // TODO: don't forget to remove it once we've paused after this, even if the BP wasn't hit (e.g. due to an exception on current instruction)
				singleStepBreakpoint->setInternal(true);
			}
			return core_->ptraceContinue(tid, status);
		}
		return Status(tr("failed to disassemble instruction at address %1.").arg(pc.toPointerString()));
	}
	return Status(tr("failed to get instruction bytes at address %1.").arg(pc.toPointerString()));
}

/**
 * steps this thread one instruction, passing the signal that stopped it
 * (unless the signal was SIGSTOP)
 *
 * @brief PlatformThread::step
 * @return
 */
Status PlatformThread::step() {
	return doStep(tid_, resume_code(status_));
}

/**
 * steps this thread one instruction, passing the signal that stopped it
 * (unless the signal was SIGSTOP, or the passed status != DEBUG_EXCEPTION_NOT_HANDLED)
 *
 * @brief PlatformThread::step
 * @param status
 * @return
 */
Status PlatformThread::step(edb::EVENT_STATUS status) {
	const int code = (status == edb::DEBUG_EXCEPTION_NOT_HANDLED) ? resume_code(status_) : 0;
	return doStep(tid_, code);
}

}
