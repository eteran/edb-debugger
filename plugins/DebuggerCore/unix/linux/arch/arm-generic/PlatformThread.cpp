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
#include "ArchProcessor.h"
#include "Breakpoint.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE        /* or _BSD_SOURCE or _SVID_SOURCE */
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
// Name: fillStateFromVFPRegs
// Desc:
//------------------------------------------------------------------------------
bool PlatformThread::fillStateFromVFPRegs(PlatformState* state) {

	user_vfp fpr;
	if(ptrace(PTRACE_GETVFPREGS, tid_, 0, &fpr) != -1) {
		for(unsigned i=0;i<sizeof fpr.fpregs/sizeof*fpr.fpregs;++i)
			state->fillFrom(fpr);
		return true;
	}
	else {
		perror("PTRACE_GETVFPREGS failed");
		return false;
	}

}

//------------------------------------------------------------------------------
// Name: get_state
// Desc:
//------------------------------------------------------------------------------
void PlatformThread::get_state(State *state) {
	// TODO: assert that we are paused

	core_->detectCPUMode();

	if(auto state_impl = static_cast<PlatformState *>(state->impl_.get())) {

		fillStateFromSimpleRegs(state_impl);
		fillStateFromVFPRegs(state_impl);
	}
}

//------------------------------------------------------------------------------
// Name: set_state
// Desc:
//------------------------------------------------------------------------------
void PlatformThread::set_state(const State &state) {

	// TODO: assert that we are paused

	if(auto state_impl = static_cast<PlatformState *>(state.impl_.get())) {

		user_regs regs;
		state_impl->fillStruct(regs);
		if(ptrace(PTRACE_SETREGS, tid_, 0, &regs) == -1) {
			perror("PTRACE_SETREGS failed");
		}

		user_vfp fpr;
		state_impl->fillStruct(fpr);
		if(ptrace(PTRACE_SETVFPREGS, tid_, 0, &fpr) == -1) {
			perror("PTRACE_SETVFPREGS failed");
		}
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

	constexpr auto addrSize = 4; // The code here is ARM32-specific anyway...

	State state;
	get_state(&state);
	if(state.empty()) return Status(QObject::tr("failed to get thread state."));
	const auto pc = state.instruction_pointer();
	const auto flags = state.flags();
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
		if(const auto insn = edb::Instruction(buffer, buffer + size, pc))
		{

			const auto op = insn.operation();
			edb::address_t addrAfterInsn=pc+insn.byte_size();

			auto targetMode = core_->cpu_mode();
			if(modifies_pc(insn) && edb::v1::arch_processor().is_executed(insn,state))
			{
				if(op == ARM_INS_BXJ)
					return Status(QObject::tr("EDB doesn't yet support single-stepping into Jazelle state."));

				const auto opCount = insn.operand_count();
				if(opCount==0)
					return Status(QObject::tr("instruction %1 isn't supported yet.").arg(insn.mnemonic().c_str()));

				switch(op)
				{
				case ARM_INS_LDR:
				{
					const auto destOperand=insn.operand(0);
					if(!is_register(destOperand) || destOperand->reg!=ARM_REG_PC)
						return Status(QObject::tr("instruction %1 with non-PC destination isn't supported yet.").arg(insn.mnemonic().c_str()));
					const auto srcOperand=insn.operand(1);
					if(!is_expression(srcOperand))
						return Status(QObject::tr("unexpected type of second operand of LDR instruction."));
					const auto effAddrR=edb::v1::arch_processor().get_effective_address(insn, srcOperand, state);
					if(!effAddrR) return Status(effAddrR.error());

					const auto effAddr=effAddrR.value();
					if(process_->read_bytes(effAddr, &addrAfterInsn, addrSize)!=addrSize)
						return Status(QObject::tr("failed to read memory referred to by LDR operand (address %1).").arg(effAddr.toPointerString()));

					// FIXME: for ARMv5 or below (without "T" in the name) bits [1:0] are simply ignored, without any mode change
					if(addrAfterInsn&1)
						targetMode=IDebugger::CPUMode::Thumb;
					else
						targetMode=IDebugger::CPUMode::ARM32;
					switch(edb::v1::debugger_core->cpu_mode())
					{
					case IDebugger::CPUMode::Thumb:
						addrAfterInsn&=-2;
						break;
					case IDebugger::CPUMode::ARM32:
						addrAfterInsn&=-4;
						break;
					default:
						return Status(QObject::tr("single-stepping LDR instruction in modes other than ARM or Thumb is not supported yet."));
					}
					break;
				}
				case ARM_INS_POP:
				{
					int i=0;
					for(;i<opCount;++i)
					{
						const auto operand=insn.operand(i);
						if(is_register(operand) && operand->reg==ARM_REG_PC)
						{
							assert(operand->access==CS_AC_WRITE);
							const auto sp=state.gp_register(PlatformState::GPR::SP);
							if(!sp) return Status(QObject::tr("failed to get value of SP register"));
							if(process_->read_bytes(sp.valueAsAddress()+addrSize*i, &addrAfterInsn, addrSize)!=addrSize)
								return Status(QObject::tr("failed to read thread stack"));
							break;
						}
					}
					if(i==opCount)
						return Status(QObject::tr("internal EDB error: failed to locate PC in the instruction operand list"));
					break;
				}
				case ARM_INS_BX:
				case ARM_INS_BLX:
				case ARM_INS_B:
				case ARM_INS_BL:
				{
					if(opCount!=1)
						return Status(QObject::tr("unexpected form of instruction %1 with %2 operands.").arg(insn.mnemonic().c_str()).arg(opCount));
					const auto& operand=insn.operand(0);
					assert(operand);
					if(is_immediate(operand))
					{
						addrAfterInsn=edb::address_t(util::to_unsigned(operand->imm));
						if(op==ARM_INS_BX || op==ARM_INS_BLX)
						{
							if(targetMode==IDebugger::CPUMode::ARM32)
								targetMode=IDebugger::CPUMode::Thumb;
							else
								targetMode=IDebugger::CPUMode::ARM32;
						}
						break;
					}
					else if(is_register(operand))
					{
						if(operand->reg==ARM_REG_PC && (op==ARM_INS_BX || op==ARM_INS_BLX))
							return Status(QObject::tr("unpredictable instruction"));
						// This may happen only with BX or BLX: B and BL require an immediate operand
						const auto result=edb::v1::arch_processor().get_effective_address(insn, operand, state);
						if(!result) return Status(result.error());
						addrAfterInsn=result.value();
						if(addrAfterInsn&1)
							targetMode=IDebugger::CPUMode::Thumb;
						else
							targetMode=IDebugger::CPUMode::ARM32;
						addrAfterInsn&=~1;
						if(addrAfterInsn&0x3 && targetMode!=IDebugger::CPUMode::Thumb)
							return Status(QObject::tr("won't try to set breakpoint at unaligned address"));
						break;
					}
					return Status(QObject::tr("bad operand for %1 instruction.").arg(insn.mnemonic().c_str()));
				}
				default:
					return Status(QObject::tr("instruction %1 modifies PC, but isn't a branch instruction known to EDB's single-stepper.").arg(insn.mnemonic().c_str()));
				}
			}

			if(singleStepBreakpoint)
				return Status(QObject::tr("internal EDB error: single-step breakpoint still present"));
			if(const auto oldBP=core_->find_breakpoint(addrAfterInsn))
			{
				// TODO: EDB should support overlapping breakpoints
				if(!oldBP->enabled())
					return Status(QObject::tr("a disabled breakpoint is present at address %1, can't set one for single step.").arg(addrAfterInsn.toPointerString()));
			}
			else
			{
				singleStepBreakpoint=core_->add_breakpoint(addrAfterInsn);
				if(!singleStepBreakpoint)
					return Status(QObject::tr("failed to set breakpoint at address %1.").arg(addrAfterInsn.toPointerString()));
				const auto bp=std::static_pointer_cast<Breakpoint>(singleStepBreakpoint);
				if(targetMode!=core_->cpu_mode())
				{
					switch(targetMode)
					{
					case IDebugger::CPUMode::ARM32:
						bp->set_type(Breakpoint::TypeId::ARM32);
						break;
					case IDebugger::CPUMode::Thumb:
						bp->set_type(Breakpoint::TypeId::Thumb2Byte);
						break;
					}
				}
				singleStepBreakpoint->set_one_time(true); // TODO: don't forget to remove it once we've paused after this, even if the BP wasn't hit (e.g. due to an exception on current instruction)
				singleStepBreakpoint->set_internal(true);
			}
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
