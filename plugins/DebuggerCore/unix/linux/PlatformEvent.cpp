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

namespace DebuggerCorePlugin {

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
		tr("<p>The debugged application encountered a %1 (%2).</p>").arg(name).arg(number),
		tr("% received").arg(name)
		);
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
IDebugEvent::Message PlatformEvent::error_description() const {
	Q_ASSERT(is_error());

	auto fault_address = edb::address_t::fromZeroExtended(siginfo_.si_addr);

	std::size_t debuggeePtrSize = edb::v1::pointer_size();
	bool fullAddressKnown=debuggeePtrSize<=sizeof(void*);
	const QString addressString = fault_address.toPointerString(fullAddressKnown);

	Message message;
	switch(code()) {
	case SIGSEGV:
		switch(siginfo_.si_code) {
		case SEGV_MAPERR:
			message=Message(
				tr("Illegal Access Fault"),
				tr("<p>The debugged application encountered a segmentation fault.<br />The address <strong>%1</strong> does not appear to be mapped.</p>").arg(addressString),
				tr("SIGSEGV: SEGV_MAPERR: Accessed address %1 not mapped").arg(addressString)
				);
			break;
		case SEGV_ACCERR:
			message=Message(
				tr("Illegal Access Fault"),
				tr("<p>The debugged application encountered a segmentation fault.<br />The address <strong>%1</strong> could not be accessed.</p>").arg(addressString),
				tr("SIGSEGV: SEGV_ACCERR: Access to address %1 not permitted").arg(addressString)
				);
			break;
		default:
			message=Message(
				tr("Illegal Access Fault"),
				tr("<p>The debugged application encountered a segmentation fault.<br />The instruction could not be executed.</p>"),
				tr("SIGSEGV: Segmentation fault")
				);
			break;
		}
		break;

	case SIGILL:
		message=Message(
			tr("Illegal Instruction Fault"),
			tr("<p>The debugged application attempted to execute an illegal instruction.</p>"),
			tr("SIGILL: Illegal instruction")
			);
		break;
	case SIGFPE:
		switch(siginfo_.si_code) {
		case FPE_INTDIV:
			message=Message(
				tr("Divide By Zero"),
				tr("<p>The debugged application tried to divide an integer value by an integer divisor of zero or encountered integer division overflow.</p>"),
				tr("SIGFPE: FPE_INTDIV: Integer division by zero or division overflow")
				);
			break;
		case FPE_FLTDIV:
			message=Message(
				tr("Divide By Zero"),
				tr("<p>The debugged application tried to divide an floating-point value by a floating-point divisor of zero.</p>"),
				tr("SIGFPE: FPE_FLTDIV: Floating-point division by zero")
				);
			break;
		case FPE_FLTOVF:
			message=Message(
				tr("Numeric Overflow"),
				tr("<p>The debugged application encountered a numeric overflow while performing a floating-point computation.</p>"),
				tr("SIGFPE: FPE_FLTOVF: Numeric overflow exception")
				);
			break;
		case FPE_FLTUND:
			message=Message(
				tr("Numeric Underflow"),
				tr("<p>The debugged application encountered a numeric underflow while performing a floating-point computation.</p>"),
				tr("SIGFPE: FPE_FLTUND: Numeric underflow exception")
				);
			break;
		case FPE_FLTRES:
			message=Message(
				tr("Inexact Result"),
				tr("<p>The debugged application encountered an inexact result of a floating-point computation it was performing.</p>"),
				tr("SIGFPE: FPE_FLTRES: Inexact result exception")
				);
			break;
		case FPE_FLTINV:
			message=Message(
				tr("Invalid Operation"),
				tr("<p>The debugged application attempted to perform an invalid floating-point operation.</p>"),
				tr("SIGFPE: FPE_FLTINV: Invalid floating-point operation")
				);
			break;
		default:
			message=Message(
				tr("Floating Point Exception"),
				tr("<p>The debugged application encountered a floating-point exception.</p>"),
				tr("SIGFPE: Floating-point exception")
				);
			break;
		}
		break;

	case SIGABRT:
		message=Message(
			tr("Application Aborted"),
			tr("<p>The debugged application has aborted.</p>"),
			tr("SIGABRT: Application aborted")
			);
		break;
	case SIGBUS:
		message=Message(
			tr("Bus Error"),
			tr("<p>The debugged application received a bus error. Typically, this means that it tried to read or write data that is misaligned.</p>"),
			tr("SIGBUS: Bus error")
			);
		break;
#ifdef SIGSTKFLT
	case SIGSTKFLT:
		message=Message(
			tr("Stack Fault"),
			tr("<p>The debugged application encountered a stack fault.</p>"),
			tr("SIGSTKFLT: Stack fault")
			);
		break;
#endif
	case SIGPIPE:
		message=Message(
			tr("Broken Pipe Fault"),
			tr("<p>The debugged application encountered a broken pipe fault.</p>"),
			tr("SIGPIPE: Pipe broken")
			);
		break;
#ifdef SIGHUP
	case SIGHUP:
		message=createUnexpectedSignalMessage("SIGHUP", SIGHUP);
		break;
#endif
#ifdef SIGINT
	case SIGINT:
		message=createUnexpectedSignalMessage("SIGINT", SIGINT);
		break;
#endif
#ifdef SIGQUIT
	case SIGQUIT:
		message=createUnexpectedSignalMessage("SIGQUIT", SIGQUIT);
		break;
#endif
#ifdef SIGTRAP
	case SIGTRAP:
		message=createUnexpectedSignalMessage("SIGTRAP", SIGTRAP);
		break;
#endif
#ifdef SIGKILL
	case SIGKILL:
		message=createUnexpectedSignalMessage("SIGKILL", SIGKILL);
		break;
#endif
#ifdef SIGUSR1
	case SIGUSR1:
		message=createUnexpectedSignalMessage("SIGUSR1", SIGUSR1);
		break;
#endif
#ifdef SIGUSR2
	case SIGUSR2:
		message=createUnexpectedSignalMessage("SIGUSR2", SIGUSR2);
		break;
#endif
#ifdef SIGALRM
	case SIGALRM:
		message=createUnexpectedSignalMessage("SIGALRM", SIGALRM);
		break;
#endif
#ifdef SIGTERM
	case SIGTERM:
		message=createUnexpectedSignalMessage("SIGTERM", SIGTERM);
		break;
#endif
#ifdef SIGCHLD
	case SIGCHLD:
		message=createUnexpectedSignalMessage("SIGCHLD", SIGCHLD);
		break;
#endif
#ifdef SIGCONT
	case SIGCONT:
		message=createUnexpectedSignalMessage("SIGCONT", SIGCONT);
		break;
#endif
#ifdef SIGSTOP
	case SIGSTOP:
		message=createUnexpectedSignalMessage("SIGSTOP", SIGSTOP);
		break;
#endif
#ifdef SIGTSTP
	case SIGTSTP:
		message=createUnexpectedSignalMessage("SIGTSTP", SIGTSTP);
		break;
#endif
#ifdef SIGTTIN
	case SIGTTIN:
		message=createUnexpectedSignalMessage("SIGTTIN", SIGTTIN);
		break;
#endif
#ifdef SIGTTOU
	case SIGTTOU:
		message=createUnexpectedSignalMessage("SIGTTOU", SIGTTOU);
		break;
#endif
#ifdef SIGURG
	case SIGURG:
		message=createUnexpectedSignalMessage("SIGURG", SIGURG);
		break;
#endif
#ifdef SIGXCPU
	case SIGXCPU:
		message=createUnexpectedSignalMessage("SIGXCPU", SIGXCPU);
		break;
#endif
#ifdef SIGXFSZ
	case SIGXFSZ:
		message=createUnexpectedSignalMessage("SIGXFSZ", SIGXFSZ);
		break;
#endif
#ifdef SIGVTALRM
	case SIGVTALRM:
		message=createUnexpectedSignalMessage("SIGVTALRM", SIGVTALRM);
		break;
#endif
#ifdef SIGPROF
	case SIGPROF:
		message=createUnexpectedSignalMessage("SIGPROF", SIGPROF);
		break;
#endif
#ifdef SIGWINCH
	case SIGWINCH:
		message=createUnexpectedSignalMessage("SIGWINCH", SIGWINCH);
		break;
#endif
#ifdef SIGIO
	case SIGIO:
		message=createUnexpectedSignalMessage("SIGIO", SIGIO);
		break;
#endif
	default:
		return Message();
	}

	message.message+="<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>";
	message.statusMessage+=". Shift+Run/Step to pass signal to the program";
	return message;
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
