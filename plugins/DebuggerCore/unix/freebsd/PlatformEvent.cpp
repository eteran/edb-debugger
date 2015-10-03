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

#include "PlatformEvent.h"
#include "edb.h"
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <kvm.h>
#include <sys/exec.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/ptrace.h>
#include <sys/signalvar.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h> // for the SIG* definitions

namespace DebuggerCore {

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
PlatformEvent::PlatformEvent() : status(0), pid(-1), tid(-1), fault_address_(0), fault_code_(0) {
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
PlatformEvent *PlatformEvent::clone() const {
	return new PlatformEvent(*this);
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
IDebugEvent::Message PlatformEvent::error_description() const {
	Q_ASSERT(is_error());

	auto fault_address = reinterpret_cast<edb::address_t>(fault_address_);

	switch(code()) {
	case SIGSEGV:
		return Message(
			tr("Illegal Access Fault"),
			tr(
				"<p>The debugged application encountered a segmentation fault.<br />The address <strong>0x%1</strong> could not be accessed.</p>"
				"<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>").arg(edb::v1::format_pointer(fault_address))
			);
	case SIGILL:
		return Message(
			tr("Illegal Instruction Fault"),
			tr(
				"<p>The debugged application attempted to execute an illegal instruction.</p>"
				"<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>")
			);
	case SIGFPE:
		switch(fault_code_) {
		case FPE_INTDIV:
		return Message(
			tr("Divide By Zero"),
			tr(
				"<p>The debugged application tried to divide an integer value by an integer divisor of zero.</p>"
				"<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>")
			);
		default:
			return Message(
				tr("Floating Point Exception"),
				tr(
					"<p>The debugged application encountered a floating-point exception.</p>"
					"<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>")
				);
		}

	case SIGABRT:
		return Message(
			tr("Application Aborted"),
			tr(
				"<p>The debugged application has aborted.</p>"
				"<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>")
			);
	case SIGBUS:
		return Message(
			tr("Bus Error"),
			tr(
				"<p>The debugged application tried to read or write data that is misaligned.</p>"
				"<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>")
			);
#ifdef SIGSTKFLT
	case SIGSTKFLT:
		return Message(
			tr("Stack Fault"),
			tr(
				"<p>The debugged application encountered a stack fault.</p>"
				"<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>")
			);
#endif
	case SIGPIPE:
		return Message(
			tr("Broken Pipe Fault"),
			tr(
				"<p>The debugged application encountered a broken pipe fault.</p>"
				"<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>")
			);
	default:
		return Message();
	}
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
IDebugEvent::REASON PlatformEvent:: reason() const {
	// this basically converts our value into a 'switchable' value for convenience

	if(stopped()) {
		return EVENT_STOPPED;
	} else if(terminated()) {
		return EVENT_TERMINATED;
	} else if(exited()) {
		return EVENT_EXITED;
	} else {
		return EVENT_UNKNOWN;
	}
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
IDebugEvent::TRAP_REASON PlatformEvent::trap_reason() const {
	switch(fault_code_) {
	case TRAP_TRACE: return TRAP_STEPPING;
	default:         return TRAP_BREAKPOINT;
	}
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
bool PlatformEvent::exited() const {
	return WIFEXITED(status) != 0;
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
bool PlatformEvent::is_error() const {
	if(stopped()) {
		switch(code()) {
		case SIGTRAP:
		case SIGSTOP:
			return false;
		case SIGSEGV:
		case SIGILL:
		case SIGFPE:
		case SIGABRT:
		case SIGBUS:
#ifdef SIGSTKFLT
		case SIGSTKFLT:
#endif
		case SIGPIPE:
			return true;
		default:
			return false;
		}
	} else {
		return false;
	}
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
bool PlatformEvent::is_kill() const {
	return stopped() && code() == SIGKILL;
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
bool PlatformEvent::is_stop() const {
	return stopped() && code() == SIGSTOP;
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
bool PlatformEvent::is_trap() const {
	return stopped() && code() == SIGTRAP;
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
bool PlatformEvent::terminated() const {
	return WIFSIGNALED(status) != 0;
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
bool PlatformEvent::stopped() const {
	return WIFSTOPPED(status) != 0;
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
edb::pid_t PlatformEvent::process() const {
	return pid;
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
edb::tid_t PlatformEvent::thread() const {
	return tid;
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
int PlatformEvent::code() const {
	if(stopped()) {
		return WSTOPSIG(status);
	}

	if(terminated()) {
		return WTERMSIG(status);
	}

	if(exited()) {
		return WEXITSTATUS(status);
	}

	return 0;
}

}
