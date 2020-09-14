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

#include "PlatformThread.h"
#include "DebuggerCore.h"
#include "IProcess.h"
#include "PlatformCommon.h"
#include "PlatformState.h"
#include "State.h"
#include <QtDebug>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* or _BSD_SOURCE or _SVID_SOURCE */
#endif

#include <asm/ldt.h>
#include <elf.h>
#include <fcntl.h>
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

/**
 * @brief PlatformThread::fillSegmentBases
 * @param state
 */
void PlatformThread::fillSegmentBases(PlatformState *state) {

	struct user_desc desc = {};

	for (size_t sregIndex = 0; sregIndex < state->seg_reg_count(); ++sregIndex) {
		const edb::seg_reg_t reg = state->x86.segRegs[sregIndex];
		if (!reg) {
			continue;
		}

		bool fromGDT = !(reg & 0x04); // otherwise the selector picks descriptor from LDT
		if (!fromGDT) {
			continue;
		}

		if (ptrace(PTRACE_GET_THREAD_AREA, tid_, reg.toUint() / LDT_ENTRY_SIZE, &desc) != -1) {
			state->x86.segRegBases[sregIndex]       = desc.base_addr;
			state->x86.segRegBasesFilled[sregIndex] = true;
		}
	}

	for (size_t sregIndex = 0; sregIndex < state->seg_reg_count(); ++sregIndex) {
		const edb::seg_reg_t sreg = state->x86.segRegs[sregIndex];
		if (sreg == core_->userCodeSegment32_ || sreg == core_->userCodeSegment64_ || sreg == core_->userStackSegment_ || (state->is64Bit() && sregIndex < PlatformState::X86::FS)) {
			state->x86.segRegBases[sregIndex]       = 0;
			state->x86.segRegBasesFilled[sregIndex] = true;
		}
	}
}

/**
 * @brief PlatformThread::fillStateFromPrStatus
 * @param state
 * @return
 */
bool PlatformThread::fillStateFromPrStatus(PlatformState *state) {

	static bool prStatusSupported = true;

	if (!prStatusSupported) {
		return false;
	}

	PrStatus_X86_64 prstat64;

	iovec prstat_iov = {&prstat64, sizeof(prstat64)};

	if (ptrace(PTRACE_GETREGSET, tid_, NT_PRSTATUS, &prstat_iov) != -1) {

		switch (prstat_iov.iov_len) {
		case sizeof(PrStatus_X86_64):
			state->fillFrom(prstat64);
			break;
		case sizeof(PrStatus_X86):
			// In this case the actual structure returned is PrStatus_X86,
			// so copy it to the correct container (reinterpret_cast would
			// cause UB in any case). Good compiler should be able to optimize this out.
			PrStatus_X86 prstat32;
			std::memcpy(&prstat32, &prstat64, sizeof(prstat32));
			state->fillFrom(prstat32);
			break;
		default:
			prStatusSupported = false;
			qWarning() << "PTRACE_GETREGSET(NT_PRSTATUS) returned unexpected length " << prstat_iov.iov_len;
			return false;
		}
	} else {
		prStatusSupported = false;
		perror("PTRACE_GETREGSET(NT_PRSTATUS) failed");
		return false;
	}

	fillSegmentBases(state);
	return true;
}

/**
 * @brief PlatformThread::fillStateFromSimpleRegs
 * @param state
 * @return
 */
bool PlatformThread::fillStateFromSimpleRegs(PlatformState *state) {

	user_regs_struct regs;
	if (ptrace(PTRACE_GETREGS, tid_, 0, &regs) != -1) {

		state->fillFrom(regs);
		fillSegmentBases(state);
		return true;
	} else {
		perror("PTRACE_GETREGS failed");
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

		// State must be cleared before filling to zero all presence flags, otherwise something
		// may remain not updated. Also, this way we'll mark all the unfilled values.
		state_impl->clear();

		if (EDB_IS_64_BIT) {
			// 64-bit GETREGS call always returns 64-bit state, so use it
			fillStateFromSimpleRegs(state_impl);
		} else if (!fillStateFromPrStatus(state_impl)) {
			// if EDB is 32 bit, use GETREGSET so that we get 64-bit state for 64-bit debuggee
			fillStateFromSimpleRegs(state_impl);
			// failing that, try to just get what we can
		}

		// First try to get full XSTATE
		X86XState xstate;
		struct iovec iov = {&xstate, sizeof(xstate)};

		long status = ptrace(PTRACE_GETREGSET, tid_, NT_X86_XSTATE, &iov);

		if (status == -1 || !state_impl->fillFrom(xstate, iov.iov_len)) {

			// No XSTATE available, get just floating point and SSE registers
			static bool getFPXRegsSupported = EDB_IS_32_BIT;

			UserFPXRegsStructX86 fpxregs;

			// This should be automatically optimized out on amd64. If not, not a big deal.
			// Avoiding conditional compilation to facilitate syntax error checking
			if (getFPXRegsSupported) {
				getFPXRegsSupported = (ptrace(PTRACE_GETFPXREGS, tid_, 0, &fpxregs) != -1);
			}

			if (getFPXRegsSupported) {
				state_impl->fillFrom(fpxregs);
			} else {
				// No GETFPXREGS: on x86 this means SSE is not supported
				//                on x86_64 FPREGS already contain SSE state
				struct user_fpregs_struct fpregs;
				status = ptrace(PTRACE_GETFPREGS, tid_, 0, &fpregs);

				if (status != -1) {
					state_impl->fillFrom(fpregs);
				} else {
					perror("PTRACE_GETFPREGS failed");
				}
			}
		}

		// debug registers
		for (std::size_t i = 0; i < 8; ++i) {
			state_impl->x86.dbgRegs[i] = getDebugRegister(i);
		}
	}
}

/**
 * @brief PlatformThread::setState
 * @param state
 */
void PlatformThread::setState(const State &state) {

	// TODO: assert that we are paused

	if (auto state_impl = static_cast<PlatformState *>(state.impl_.get())) {
		bool setPrStatusDone = false;

		if (EDB_IS_32_BIT && state_impl->is64Bit()) {
			// Try to set 64-bit state
			PrStatus_X86_64 prstat64;
			state_impl->fillStruct(prstat64);

			struct iovec prstat_iov = {&prstat64, sizeof(prstat64)};
			if (ptrace(PTRACE_SETREGSET, tid_, NT_PRSTATUS, &prstat_iov) != -1) {
				setPrStatusDone = true;
			} else {
				perror("PTRACE_SETREGSET failed");
			}
		}

		// Fallback to setting 32-bit set
		if (!setPrStatusDone) {
			struct user_regs_struct regs;
			state_impl->fillStruct(regs);
			ptrace(PTRACE_SETREGS, tid_, 0, &regs);
		}

		// debug registers
		for (std::size_t i = 0; i < 8; ++i) {
			setDebugRegister(i, state_impl->x86.dbgRegs[i]);
		}

		// hope for the best, adjust for reality
		static bool xsaveSupported = true;

		if (xsaveSupported) {
			X86XState xstate;
			const auto size  = state_impl->fillStruct(xstate);
			struct iovec iov = {&xstate, size};
			if (ptrace(PTRACE_SETREGSET, tid_, NT_X86_XSTATE, &iov) == -1) {
				xsaveSupported = false;
			}
		}

		// If xsave/xrstor appears unsupported, fallback to fxrstor
		// NOTE: it's not "else", it's an independent check for possibly modified flag
		if (!xsaveSupported) {
			static bool setFPXRegsSupported = EDB_IS_32_BIT;
			if (setFPXRegsSupported) {
				UserFPXRegsStructX86 fpxregs;
				state_impl->fillStruct(fpxregs);
				setFPXRegsSupported = (ptrace(PTRACE_SETFPXREGS, tid_, 0, &fpxregs) != -1);
			}

			if (!setFPXRegsSupported) {
				// No SETFPXREGS: on x86 this means SSE is not supported
				//                on x86_64 FPREGS already contain SSE state
				// Just set fpregs then
				struct user_fpregs_struct fpregs;
				state_impl->fillStruct(fpregs);
				if (ptrace(PTRACE_SETFPREGS, tid_, 0, &fpregs) == -1) {
					perror("PTRACE_SETFPREGS failed");
				}
			}
		}
	}
}

/**
 * @brief PlatformThread::instructionPointer
 * @return
 */
edb::address_t PlatformThread::instructionPointer() const {
#if defined(EDB_X86)
	return ptrace(PTRACE_PEEKUSER, tid_, offsetof(UserRegsStructX86, eip), 0);
#elif defined(EDB_X86_64)
	// NOTE(eteran): even when we debug a 32-bit app on a 64-bit debugger,
	// we use the 64-bit register struct here
	return ptrace(PTRACE_PEEKUSER, tid_, offsetof(UserRegsStructX86_64, rip), 0);
#elif defined(EDB_ARM32)
	return 0;
#elif defined(EDB_ARM64)
	return 0;
#endif
}

/**
 * @brief PlatformThread::getDebugRegister
 * @param n
 * @return
 */
unsigned long PlatformThread::getDebugRegister(std::size_t n) {
	size_t drOffset = offsetof(struct user, u_debugreg) + n * sizeof(user::u_debugreg[0]);
	return ptrace(PTRACE_PEEKUSER, tid_, drOffset, 0);
}

/**
 * @brief PlatformThread::setDebugRegister
 * @param n
 * @param value
 * @return
 */
long PlatformThread::setDebugRegister(std::size_t n, unsigned long value) {
	size_t drOffset = offsetof(struct user, u_debugreg) + n * sizeof(user::u_debugreg[0]);
	return ptrace(PTRACE_POKEUSER, tid_, drOffset, value);
}

/**
 * steps this thread one instruction, passing the signal that stopped it
 * (unless the signal was SIGSTOP)
 *
 * @brief PlatformThread::step
 * @return
 */
Status PlatformThread::step() {
	return core_->ptraceStep(tid_, resume_code(status_));
}

/**
 * steps this thread one instruction, passing the signal that stopped it
 * (unless the signal was SIGSTOP, or the passed status != DEBUG_EXCEPTION_NOT_HANDLED)
 *
 * @brief PlatformThread::step
 * @param status
 * @return
 */
Status PlatformThread::step(edb::EventStatus status) {
	const int code = (status == edb::DEBUG_EXCEPTION_NOT_HANDLED) ? resume_code(status_) : 0;
	return core_->ptraceStep(tid_, code);
}

}
