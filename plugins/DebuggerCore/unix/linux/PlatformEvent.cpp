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

namespace DebuggerCore {

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
PlatformEvent::PlatformEvent() : pid_(0), tid_(0), status_(0) {
	std::memset(&siginfo_, 0, sizeof(siginfo_t));
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
IDebugEvent *PlatformEvent::clone() const {
	return new PlatformEvent(*this);
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
IDebugEvent::Message PlatformEvent::createUnexpectedSignalMessage(const QString &name, int number) {
	return Message(
		tr("Unexpected Signal Encountered"),
		tr(
			"<p>The debugged application encountered a %1 (%2).</p>"
			"<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>").arg(name).arg(number)
		);
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
IDebugEvent::Message PlatformEvent::error_description() const {
	Q_ASSERT(is_error());

	auto fault_address = edb::address_t::fromZeroExtended(siginfo_.si_addr);

	std::size_t debuggeePtrSize=edb::v1::pointer_size();
	bool fullAddressKnown=debuggeePtrSize<=sizeof(void*);

	switch(code()) {
	case SIGSEGV:
		switch(siginfo_.si_code) {
		case SEGV_MAPERR:
			return Message(
				tr("Illegal Access Fault"),
				tr(
					"<p>The debugged application encountered a segmentation fault.<br />The address <strong>%1</strong> does not appear to be mapped.</p>"
					"<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>").arg(fault_address.toPointerString(fullAddressKnown))
				);
		case SEGV_ACCERR:
			return Message(
				tr("Illegal Access Fault"),
				tr(
					"<p>The debugged application encountered a segmentation fault.<br />The address <strong>%1</strong> could not be accessed.</p>"
					"<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>").arg(fault_address.toPointerString(fullAddressKnown))
				);
		default:
			return Message(
				tr("Illegal Access Fault"),
				tr(
					"<p>The debugged application encountered a segmentation fault.<br />The instruction could not be executed.</p>"
					"<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>")
				);
		}

	case SIGILL:
		return Message(
			tr("Illegal Instruction Fault"),
			tr(
				"<p>The debugged application attempted to execute an illegal instruction.</p>"
				"<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>")
			);
	case SIGFPE:
		switch(siginfo_.si_code) {
		case FPE_INTDIV:
		return Message(
			tr("Divide By Zero"),
			tr(
				"<p>The debugged application tried to divide an integer value by an integer divisor of zero or encountered integer division overflow.</p>"
				"<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>")
			);
		case FPE_FLTDIV:
		return Message(
			tr("Divide By Zero"),
			tr(
				"<p>The debugged application tried to divide an floating-point value by a floating-point divisor of zero.</p>"
				"<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>")
			);
		case FPE_FLTOVF:
		return Message(
			tr("Numeric Overflow"),
			tr(
				"<p>The debugged application encountered a numeric overflow while performing a floating-point computation.</p>"
				"<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>")
			);
		case FPE_FLTUND:
		return Message(
			tr("Numeric Underflow"),
			tr(
				"<p>The debugged application encountered a numeric underflow while performing a floating-point computation.</p>"
				"<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>")
			);
		case FPE_FLTRES:
		return Message(
			tr("Inexact Result"),
			tr(
				"<p>The debugged application encountered an inexact result of a floating-point computation it was performing.</p>"
				"<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>")
			);
		case FPE_FLTINV:
		return Message(
			tr("Invalid Operation"),
			tr(
				"<p>The debugged application attempted to perform an invalid floating-point operation.</p>"
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
				"<p>The debugged application received a bus error. Typically, this means that it tried to read or write data that is misaligned.</p>"
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
#ifdef SIGHUP
	case SIGHUP:
		return createUnexpectedSignalMessage("SIGHUP", SIGHUP);
#endif
#ifdef SIGINT
	case SIGINT:
		return createUnexpectedSignalMessage("SIGINT", SIGINT);
#endif
#ifdef SIGQUIT
	case SIGQUIT:
		return createUnexpectedSignalMessage("SIGQUIT", SIGQUIT);
#endif
#ifdef SIGTRAP
	case SIGTRAP:
		return createUnexpectedSignalMessage("SIGTRAP", SIGTRAP);
#endif
#ifdef SIGKILL
	case SIGKILL:
		return createUnexpectedSignalMessage("SIGKILL", SIGKILL);
#endif
#ifdef SIGUSR1
	case SIGUSR1:
		return createUnexpectedSignalMessage("SIGUSR1", SIGUSR1);
#endif
#ifdef SIGUSR2
	case SIGUSR2:
		return createUnexpectedSignalMessage("SIGUSR2", SIGUSR2);
#endif
#ifdef SIGALRM
	case SIGALRM:
		return createUnexpectedSignalMessage("SIGALRM", SIGALRM);
#endif
#ifdef SIGTERM
	case SIGTERM:
		return createUnexpectedSignalMessage("SIGTERM", SIGTERM);
#endif
#ifdef SIGCHLD
	case SIGCHLD:
		return createUnexpectedSignalMessage("SIGCHLD", SIGCHLD);
#endif
#ifdef SIGCONT
	case SIGCONT:
		return createUnexpectedSignalMessage("SIGCONT", SIGCONT);
#endif
#ifdef SIGSTOP
	case SIGSTOP:
		return createUnexpectedSignalMessage("SIGSTOP", SIGSTOP);
#endif
#ifdef SIGTSTP
	case SIGTSTP:
		return createUnexpectedSignalMessage("SIGTSTP", SIGTSTP);
#endif
#ifdef SIGTTIN
	case SIGTTIN:
		return createUnexpectedSignalMessage("SIGTTIN", SIGTTIN);
#endif
#ifdef SIGTTOU
	case SIGTTOU:
		return createUnexpectedSignalMessage("SIGTTOU", SIGTTOU);
#endif
#ifdef SIGURG
	case SIGURG:
		return createUnexpectedSignalMessage("SIGURG", SIGURG);
#endif
#ifdef SIGXCPU
	case SIGXCPU:
		return createUnexpectedSignalMessage("SIGXCPU", SIGXCPU);
#endif
#ifdef SIGXFSZ
	case SIGXFSZ:
		return createUnexpectedSignalMessage("SIGXFSZ", SIGXFSZ);
#endif
#ifdef SIGVTALRM
	case SIGVTALRM:
		return createUnexpectedSignalMessage("SIGVTALRM", SIGVTALRM);
#endif
#ifdef SIGPROF
	case SIGPROF:
		return createUnexpectedSignalMessage("SIGPROF", SIGPROF);
#endif
#ifdef SIGWINCH
	case SIGWINCH:
		return createUnexpectedSignalMessage("SIGWINCH", SIGWINCH);
#endif
#ifdef SIGIO
	case SIGIO:
		return createUnexpectedSignalMessage("SIGIO", SIGIO);
#endif
	default:
		return Message();
	}
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
IDebugEvent::REASON PlatformEvent::reason() const {
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
	switch(siginfo_.si_code) {
	case TRAP_TRACE: return TRAP_STEPPING;
	default:         return TRAP_BREAKPOINT;
	}
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
bool PlatformEvent::exited() const {
	return WIFEXITED(status_) != 0;
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
	return WIFSIGNALED(status_) != 0;
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
bool PlatformEvent::stopped() const {
	return WIFSTOPPED(status_) != 0;
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
edb::pid_t PlatformEvent::process() const {
	return pid_;
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
edb::tid_t PlatformEvent::thread() const {
	return tid_;
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
int PlatformEvent::code() const {
	if(stopped()) {
		return WSTOPSIG(status_);
	}

	if(terminated()) {
		return WTERMSIG(status_);
	}

	if(exited()) {
		return WEXITSTATUS(status_);
	}

	return 0;
}

}
