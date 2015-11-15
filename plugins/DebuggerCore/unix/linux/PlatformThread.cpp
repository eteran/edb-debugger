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
#include "PlatformState.h"
#include "PlatformCommon.h"
#include "IProcess.h"
#include "DebuggerCore.h"
#include "State.h"
#include <QtDebug>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE        /* or _BSD_SOURCE or _SVID_SOURCE */
#endif

#include <asm/ldt.h>
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

namespace DebuggerCore {

//------------------------------------------------------------------------------
// Name: fillSegmentBases
// Desc: 
//------------------------------------------------------------------------------
void PlatformThread::fillSegmentBases(PlatformState* state) {

	struct user_desc desc;
	std::memset(&desc, 0, sizeof(desc));

	for(size_t sregIndex=0;sregIndex<state->seg_reg_count();++sregIndex) {
		const edb::seg_reg_t reg=state->x86.segRegs[sregIndex];
		if(!reg) continue;
		bool fromGDT=!(reg&0x04); // otherwise the selector picks descriptor from LDT
		if(!fromGDT) continue;
		if(ptrace(PTRACE_GET_THREAD_AREA, tid_, reg / LDT_ENTRY_SIZE, &desc) != -1) {
			state->x86.segRegBases[sregIndex] = desc.base_addr;
			state->x86.segRegBasesFilled[sregIndex]=true;
		}
	}
	for(size_t sregIndex=0;sregIndex<state->seg_reg_count();++sregIndex) {
		const edb::seg_reg_t sreg=state->x86.segRegs[sregIndex];
		if(sreg==core_->USER_CS_32||sreg==core_->USER_CS_64||sreg==core_->USER_SS ||
				(state->is64Bit() && sregIndex<PlatformState::X86::FS)) {
			state->x86.segRegBases[sregIndex] = 0;
			state->x86.segRegBasesFilled[sregIndex]=true;
		}
	}
}

//------------------------------------------------------------------------------
// Name: fillStateFromPrStatus
// Desc: 
//------------------------------------------------------------------------------
bool PlatformThread::fillStateFromPrStatus(PlatformState* state) {

	static bool prStatusSupported=true;
	if(!prStatusSupported)
		return false;


	PrStatus_X86_64 prstat64;

	iovec prstat_iov = {&prstat64, sizeof(prstat64)};

	if(ptrace(PTRACE_GETREGSET, tid_, NT_PRSTATUS, &prstat_iov) != -1) {
		if(prstat_iov.iov_len==sizeof(prstat64)) {
			state->fillFrom(prstat64);
		} else if(prstat_iov.iov_len==sizeof(PrStatus_X86)) {
			// In this case the actual structure returned is PrStatus_X86,
			// so copy it to the correct container (reinterpret_cast would
			// cause UB in any case). Good compiler should be able to optimize this out.
			PrStatus_X86 prstat32;
			std::memcpy(&prstat32,&prstat64,sizeof(prstat32));
			state->fillFrom(prstat32);
		} else {
			prStatusSupported=false;
			qWarning() << "PTRACE_GETREGSET(NT_PRSTATUS) returned unexpected length " << prstat_iov.iov_len;
			return false;
		}
	}
	else {
		prStatusSupported=false;
		perror("PTRACE_GETREGSET(NT_PRSTATUS) failed");
		return false;
	}
	fillSegmentBases(state);
	return true;
}

//------------------------------------------------------------------------------
// Name: fillStateFromSimpleRegs
// Desc: 
//------------------------------------------------------------------------------
bool PlatformThread::fillStateFromSimpleRegs(PlatformState* state) {

	user_regs_struct regs;
	if(ptrace(PTRACE_GETREGS, tid_, 0, &regs) != -1) {

		state->fillFrom(regs);
		fillSegmentBases(state);
		return true;
	}
	else {
		perror("PTRACE_GETREGS failed");
		return false;
	}
}


//------------------------------------------------------------------------------
// Name: PlatformThread
// Desc: 
//------------------------------------------------------------------------------
PlatformThread::PlatformThread(DebuggerCore *core, IProcess *process, edb::tid_t tid) : core_(core), process_(process), tid_(tid) {
	assert(process);
	assert(core);
}

//------------------------------------------------------------------------------
// Name: 
// Desc: 
//------------------------------------------------------------------------------
PlatformThread::~PlatformThread() {
}

//------------------------------------------------------------------------------
// Name: 
// Desc: 
//------------------------------------------------------------------------------
edb::tid_t PlatformThread::tid() const {
	return tid_;
}

//------------------------------------------------------------------------------
// Name: 
// Desc: 
//------------------------------------------------------------------------------
QString PlatformThread::name() const  {
	struct user_stat thread_stat;
	int n = get_user_stat(QString("/proc/%1/task/%2/stat").arg(process_->pid()).arg(tid_), &thread_stat);
	if(n >= 2) {
		return thread_stat.comm;
	}
	
	return QString();
}

//------------------------------------------------------------------------------
// Name: 
// Desc: 
//------------------------------------------------------------------------------
int PlatformThread::priority() const  {
	struct user_stat thread_stat;
	int n = get_user_stat(QString("/proc/%1/task/%2/stat").arg(process_->pid()).arg(tid_), &thread_stat);
	if(n >= 30) {
		return thread_stat.priority;
	}
	
	return 0;
}

//------------------------------------------------------------------------------
// Name: 
// Desc: 
//------------------------------------------------------------------------------
edb::address_t PlatformThread::instruction_pointer() const  {
	struct user_stat thread_stat;
	int n = get_user_stat(QString("/proc/%1/task/%2/stat").arg(process_->pid()).arg(tid_), &thread_stat);
	if(n >= 18) {
		return thread_stat.kstkeip;
	}
	
	return 0;
}

//------------------------------------------------------------------------------
// Name: 
// Desc: 
//------------------------------------------------------------------------------
QString PlatformThread::runState() const  {
	struct user_stat thread_stat;
	int n = get_user_stat(QString("/proc/%1/task/%2/stat").arg(process_->pid()).arg(tid_), &thread_stat);
	if(n >= 3) {
		switch(thread_stat.state) {           // 03
		case 'R':
			return tr("%1 (Running)").arg(thread_stat.state);
			break;
		case 'S':
			return tr("%1 (Sleeping)").arg(thread_stat.state);
			break;
		case 'D':
			return tr("%1 (Disk Sleep)").arg(thread_stat.state);
			break;		
		case 'T':
			return tr("%1 (Stopped)").arg(thread_stat.state);
			break;		
		case 't':
			return tr("%1 (Tracing Stop)").arg(thread_stat.state);
			break;		
		case 'Z':
			return tr("%1 (Zombie)").arg(thread_stat.state);
			break;		
		case 'X':
		case 'x':
			return tr("%1 (Dead)").arg(thread_stat.state);
			break;
		case 'W':
			return tr("%1 (Waking/Paging)").arg(thread_stat.state);
			break;			
		case 'K':
			return tr("%1 (Wakekill)").arg(thread_stat.state);
			break;		
		case 'P':
			return tr("%1 (Parked)").arg(thread_stat.state);
			break;		
		default:
			return tr("%1").arg(thread_stat.state);
			break;		
		} 
	}
	
	return tr("Unknown");
}

//------------------------------------------------------------------------------
// Name: step
// Desc: steps this thread one instruction, passing the signal that stopped it 
//       (unless the signal was SIGSTOP)
//------------------------------------------------------------------------------
void PlatformThread::step() {
	core_->ptrace_step(tid_, resume_code(status_));
}

//------------------------------------------------------------------------------
// Name: step
// Desc: steps this thread one instruction, passing the signal that stopped it 
//       (unless the signal was SIGSTOP, or the passed status != DEBUG_EXCEPTION_NOT_HANDLED)
//------------------------------------------------------------------------------
void PlatformThread::step(edb::EVENT_STATUS status) {
	const int code = (status == edb::DEBUG_EXCEPTION_NOT_HANDLED) ? resume_code(status_) : 0;
	core_->ptrace_step(tid_, code);
}

//------------------------------------------------------------------------------
// Name: resume
// Desc: resumes this thread, passing the signal that stopped it 
//       (unless the signal was SIGSTOP)
//------------------------------------------------------------------------------	
void PlatformThread::resume() {
	core_->ptrace_continue(tid_, resume_code(status_));
}

//------------------------------------------------------------------------------
// Name: resume
// Desc: resumes this thread , passing the signal that stopped it 
//       (unless the signal was SIGSTOP, or the passed status != DEBUG_EXCEPTION_NOT_HANDLED)
//------------------------------------------------------------------------------
void PlatformThread::resume(edb::EVENT_STATUS status) {
	const int code = (status == edb::DEBUG_EXCEPTION_NOT_HANDLED) ? resume_code(status_) : 0;
	core_->ptrace_continue(tid_, code);
}

//------------------------------------------------------------------------------
// Name: get_state
// Desc:
//------------------------------------------------------------------------------
void PlatformThread::get_state(State *state) {
	// TODO: assert that we are paused

	core_->detectDebuggeeBitness();

	if(auto state_impl = static_cast<PlatformState *>(state->impl_)) {
		// State must be cleared before filling to zero all presence flags, otherwise something
		// may remain not updated. Also, this way we'll mark all the unfilled values.
		state_impl->clear();

			if(EDB_IS_64_BIT)
				fillStateFromSimpleRegs(state_impl); // 64-bit GETREGS call always returns 64-bit state, so use it
			else if(!fillStateFromPrStatus(state_impl)) // if EDB is 32 bit, use GETREGSET so that we get 64-bit state for 64-bit debuggee
				fillStateFromSimpleRegs(state_impl); // failing that, try to just get what we can

			long ptraceStatus=0;

			// First try to get full XSTATE
			X86XState xstate;
			iovec iov={&xstate,sizeof(xstate)};
			ptraceStatus=ptrace(PTRACE_GETREGSET, tid_, NT_X86_XSTATE, &iov);
			if(ptraceStatus!=-1) {
				state_impl->fillFrom(xstate,iov.iov_len);
			} else {

				// No XSTATE available, get just floating point and SSE registers
				static bool getFPXRegsSupported=(EDB_IS_32_BIT ? true : false);
				UserFPXRegsStructX86 fpxregs;
				// This should be automatically optimized out on amd64. If not, not a big deal.
				// Avoiding conditional compilation to facilitate syntax error checking
				if(getFPXRegsSupported)
					getFPXRegsSupported=(ptrace(PTRACE_GETFPXREGS, tid_, 0, &fpxregs)!=-1);

				if(getFPXRegsSupported) {
					state_impl->fillFrom(fpxregs);
				} else {
					// No GETFPXREGS: on x86 this means SSE is not supported
					//                on x86_64 FPREGS already contain SSE state
					user_fpregs_struct fpregs;
					if((ptraceStatus=ptrace(PTRACE_GETFPREGS, tid_, 0, &fpregs))!=-1)
						state_impl->fillFrom(fpregs);
					else
						perror("PTRACE_GETFPREGS failed");
				}
			}

			// debug registers
			for(std::size_t i=0;i<8;++i)
				state_impl->x86.dbgRegs[i] = get_debug_register(i);
	}
}

//------------------------------------------------------------------------------
// Name: set_state
// Desc:
//------------------------------------------------------------------------------
void PlatformThread::set_state(const State &state) {

	// TODO: assert that we are paused
	
	if(auto state_impl = static_cast<PlatformState *>(state.impl_)) {
		bool setPrStatusDone=false;
		if(EDB_IS_32_BIT && state_impl->is64Bit()) {
			// Try to set 64-bit state
			PrStatus_X86_64 prstat64;
			state_impl->fillStruct(prstat64);
			iovec prstat_iov = {&prstat64, sizeof(prstat64)};
			if(ptrace(PTRACE_SETREGSET, tid_, NT_PRSTATUS, &prstat_iov) != -1)
				setPrStatusDone=true;
			else
				perror("PTRACE_SETREGSET failed");
		}
		// Fallback to setting 32-bit set
		if(!setPrStatusDone) {
			user_regs_struct regs;
			state_impl->fillStruct(regs);
			ptrace(PTRACE_SETREGS, tid_, 0, &regs);
		}

		// debug registers
		for(std::size_t i=0;i<8;++i)
			set_debug_register(i,state_impl->x86.dbgRegs[i]);

		static bool xsaveSupported=true; // hope for the best, adjust for reality
		if(xsaveSupported) {
			X86XState xstate;
			const auto size=state_impl->fillStruct(xstate);
			iovec iov={&xstate,size};
			if(ptrace(PTRACE_SETREGSET, tid_, NT_X86_XSTATE, &iov)==-1)
				xsaveSupported=false;
		}
		// If xsave/xrstor appears unsupported, fallback to fxrstor
		// NOTE: it's not "else", it's an independent check for possibly modified flag
		if(!xsaveSupported) {
			static bool setFPXRegsSupported=EDB_IS_32_BIT;
			if(setFPXRegsSupported) {
				UserFPXRegsStructX86 fpxregs;
				state_impl->fillStruct(fpxregs);
				setFPXRegsSupported=(ptrace(PTRACE_SETFPXREGS, tid_, 0, &fpxregs)!=-1);
			}
			if(!setFPXRegsSupported) {
				// No SETFPXREGS: on x86 this means SSE is not supported
				//                on x86_64 FPREGS already contain SSE state
				// Just set fpregs then
				user_fpregs_struct fpregs;
				state_impl->fillStruct(fpregs);
				if(ptrace(PTRACE_SETFPREGS, tid_, 0, &fpregs)==-1)
					perror("PTRACE_SETFPREGS failed");
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: get_debug_register
// Desc:
//------------------------------------------------------------------------------
unsigned long PlatformThread::get_debug_register(std::size_t n) {
	return ptrace(PTRACE_PEEKUSER, tid_, offsetof(struct user, u_debugreg[n]), 0);
}

//------------------------------------------------------------------------------
// Name: set_debug_register
// Desc:
//------------------------------------------------------------------------------
long PlatformThread::set_debug_register(std::size_t n, long value) {
	return ptrace(PTRACE_POKEUSER, tid_, offsetof(struct user, u_debugreg[n]), value);
}

//------------------------------------------------------------------------------
// Name: stop
// Desc:
//------------------------------------------------------------------------------
void PlatformThread::stop() {
	syscall(SYS_tgkill, process_->pid(), tid_, SIGSTOP);
	// TODO(eteran): should this just be this?
	//::kill(tid_, SIGSTOP);
}


}
