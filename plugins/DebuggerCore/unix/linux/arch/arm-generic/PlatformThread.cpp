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
#include "DebuggerCore.h"
#include "IProcess.h"
#include "PlatformCommon.h"
#include "PlatformState.h"
#include <QtDebug>
#include "State.h"
#include "Types.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE        /* or _BSD_SOURCE or _SVID_SOURCE */
#endif

#include <elf.h>
#include <linux/uio.h>
#include <sys/ptrace.h>
#include <sys/user.h>


// doesn't always seem to be defined in the headers
#ifndef PTRACE_GET_THREAD_AREA
#define PTRACE_GET_THREAD_AREA static_cast<__ptrace_request>(25)
#endif

#ifndef PTRACE_SET_THREAD_AREA
#define PTRACE_SET_THREAD_AREA static_cast<__ptrace_request>(26)
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

namespace DebuggerCorePlugin {

//------------------------------------------------------------------------------
// Name: fillStateFromPrStatus
// Desc:
//------------------------------------------------------------------------------
bool PlatformThread::fillStateFromPrStatus(PlatformState* state) {

	return false;
}

//------------------------------------------------------------------------------
// Name: fillStateFromSimpleRegs
// Desc:
//------------------------------------------------------------------------------
bool PlatformThread::fillStateFromSimpleRegs(PlatformState* state) {

	user_regs regs;
	if(ptrace(PTRACE_GETREGS, tid_, 0, &regs) != -1) {

		state->fillFrom(regs);
		return true;
	}
	else {
		perror("PTRACE_GETREGS failed");
		return false;
	}
}

//------------------------------------------------------------------------------
// Name: get_state
// Desc:
//------------------------------------------------------------------------------
void PlatformThread::get_state(State *state) {
	// TODO: assert that we are paused

	core_->detectDebuggeeBitness();

	if(auto state_impl = static_cast<PlatformState *>(state->impl_)) {

		fillStateFromSimpleRegs(state_impl);
	}
}

//------------------------------------------------------------------------------
// Name: set_state
// Desc:
//------------------------------------------------------------------------------
void PlatformThread::set_state(const State &state) {

	// TODO: assert that we are paused

	if(auto state_impl = static_cast<PlatformState *>(state.impl_)) {

	}
}

//------------------------------------------------------------------------------
// Name: get_debug_register
// Desc:
//------------------------------------------------------------------------------
unsigned long PlatformThread::get_debug_register(std::size_t n) {
	return 0;
}

//------------------------------------------------------------------------------
// Name: set_debug_register
// Desc:
//------------------------------------------------------------------------------
long PlatformThread::set_debug_register(std::size_t n, long value) {
	return 0;
}

Status PlatformThread::doStep(const edb::tid_t tid, const long status) {

	State state;
	get_state(&state);
	if(state.empty()) return Status(QObject::tr("failed to get thread state."));
	const auto pc=state.instruction_pointer();
	const auto flags=state.flags();
	enum {
		CPSR_Tbit     = 1<<5,

		CPSR_ITbits72 = 1<<10,
		CPSR_ITmask72 = 0xfc00,

		CPSR_Jbit     = 1<<24,

		CPSR_ITbits10 = 1<<25,
		CPSR_ITmask10 = 0x06000000,
	};
	if(flags & CPSR_Jbit)
		return Status(QObject::tr("EDB doesn't yet support single-stepping in Jazelle state."));
	if(flags&CPSR_Tbit && flags&(CPSR_ITmask10 | CPSR_ITmask72))
		return Status(QObject::tr("EDB doesn't yet support single-stepping inside Thumb-2 IT-block."));
	quint8 buffer[4];
	if(const int size = edb::v1::get_instruction_bytes(pc, buffer))
	{
		if(const auto insn=edb::Instruction(buffer, buffer + size, pc))
		{

			const auto op=insn.operation();
			if(op==ARM_INS_BXJ)
				return Status(QObject::tr("EDB doesn't yet support single-stepping into Jazelle state."));
			if(op==ARM_INS_BX || op==ARM_INS_BLX)
				return Status(QObject::tr("EDB doesn't yet support single-stepping into/out of Thumb state."));

			edb::address_t addrAfterInsn=pc+insn.byte_size();

			if(modifies_pc(insn))
			{
				const auto opCount=insn.operand_count();
				if(opCount==0)
					return Status(QObject::tr("instruction %1 isn't supported yet.").arg(insn.mnemonic().c_str()));
				switch(op)
				{
				case ARM_INS_B:
				case ARM_INS_BL:
				{
					if(opCount!=1)
						return Status(QObject::tr("unexpected form of branch instruction with %1 operands.").arg(opCount));
					if(insn.condition_code()!=edb::Instruction::CC_AL)
						return Status(QObject::tr("conditional instructions aren't supported yet."));
					const auto& op=insn.operand(0);
					assert(op);
					if(is_immediate(op))
					{
						addrAfterInsn=edb::address_t(util::to_unsigned(op->imm));
						break;
					}

					return Status(QObject::tr("EDB doesn't yet support indirect branch instructions."));
				}
				default:
					return Status(QObject::tr("instruction %1 modifies PC, but isn't a branch instruction known to EDB's single-stepper.").arg(insn.mnemonic().c_str()));
				}
			}

			singleStepBreakpoint=core_->add_breakpoint(addrAfterInsn);
			if(!singleStepBreakpoint)
				return Status(QObject::tr("failed to set breakpoint at address %1.").arg(addrAfterInsn.toPointerString()));
			singleStepBreakpoint->set_one_time(true); // TODO: don't forget to remove it once we've paused after this, even if the BP wasn't hit (e.g. due to an exception on current instruction)
			singleStepBreakpoint->set_internal(true);
			return core_->ptrace_continue(tid, status);
		}
		return Status(QObject::tr("failed to disassemble instruction at address %1.").arg(pc.toPointerString()));
	}
	return Status(QObject::tr("failed to get instruction bytes at address %1.").arg(pc.toPointerString()));
}

//------------------------------------------------------------------------------
// Name: step
// Desc: steps this thread one instruction, passing the signal that stopped it
//       (unless the signal was SIGSTOP)
//------------------------------------------------------------------------------
Status PlatformThread::step() {
	return doStep(tid_, resume_code(status_));
}

//------------------------------------------------------------------------------
// Name: step
// Desc: steps this thread one instruction, passing the signal that stopped it
//       (unless the signal was SIGSTOP, or the passed status != DEBUG_EXCEPTION_NOT_HANDLED)
//------------------------------------------------------------------------------
Status PlatformThread::step(edb::EVENT_STATUS status) {
	const int code = (status == edb::DEBUG_EXCEPTION_NOT_HANDLED) ? resume_code(status_) : 0;
	return doStep(tid_, code);
}

}
