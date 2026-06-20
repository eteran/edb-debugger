/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "PlatformEvent.h"
#include "edb.h"
#include <csignal> // for the SIG* definitions
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

namespace DebuggerCorePlugin {

/**
 * @brief Constructs a new PlatformEvent with default values.
 */
PlatformEvent::PlatformEvent()
	: status(0), pid(-1), tid(-1), fault_address_(0), fault_code_(0) {
}

/**
 * @brief Creates a new PlatformEvent instance that is a copy of the current instance.
 *
 * @return A pointer to the newly created PlatformEvent instance.
 */
PlatformEvent *PlatformEvent::clone() const {
	return new PlatformEvent(*this);
}

/**
 * @brief Returns a user-friendly error message describing the reason for the debug event, if it is an error event.
 *
 * @return A Message object containing the error description.
 */
IDebugEvent::Message PlatformEvent::error_description() const {
	Q_ASSERT(is_error());

	auto fault_address = reinterpret_cast<edb::address_t>(fault_address_);

	switch (code()) {
	case SIGSEGV:
		return Message(
			tr("Illegal Access Fault"),
			tr(
				"<p>The debugged application encountered a segmentation fault.<br />The address <strong>0x%1</strong> could not be accessed.</p>"
				"<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>")
				.arg(edb::v1::format_pointer(fault_address)));
	case SIGILL:
		return Message(
			tr("Illegal Instruction Fault"),
			tr(
				"<p>The debugged application attempted to execute an illegal instruction.</p>"
				"<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>"));
	case SIGFPE:
		switch (fault_code_) {
		case FPE_INTDIV:
			return Message(
				tr("Divide By Zero"),
				tr(
					"<p>The debugged application tried to divide an integer value by an integer divisor of zero.</p>"
					"<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>"));
		default:
			return Message(
				tr("Floating Point Exception"),
				tr(
					"<p>The debugged application encountered a floating-point exception.</p>"
					"<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>"));
		}

	case SIGABRT:
		return Message(
			tr("Application Aborted"),
			tr(
				"<p>The debugged application has aborted.</p>"
				"<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>"));
	case SIGBUS:
		return Message(
			tr("Bus Error"),
			tr(
				"<p>The debugged application tried to read or write data that is misaligned.</p>"
				"<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>"));
#ifdef SIGSTKFLT
	case SIGSTKFLT:
		return Message(
			tr("Stack Fault"),
			tr(
				"<p>The debugged application encountered a stack fault.</p>"
				"<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>"));
#endif
	case SIGPIPE:
		return Message(
			tr("Broken Pipe Fault"),
			tr(
				"<p>The debugged application encountered a broken pipe fault.</p>"
				"<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>"));
	default:
		return Message();
	}
}

/**
 * @brief Returns the reason for the debug event as an enumerated value.
 *
 * @return The reason for the debug event.
 */
IDebugEvent::REASON PlatformEvent::reason() const {
	// this basically converts our value into a 'switchable' value for convenience

	if (stopped()) {
		return EVENT_STOPPED;
	} else if (terminated()) {
		return EVENT_TERMINATED;
	} else if (exited()) {
		return EVENT_EXITED;
	} else {
		return EVENT_UNKNOWN;
	}
}

/**
 * @brief Returns the trap reason for the debug event if it is a trap event.
 *
 * @return The trap reason for the debug event.
 */
IDebugEvent::TRAP_REASON PlatformEvent::trap_reason() const {
	switch (fault_code_) {
	case TRAP_TRACE:
		return TRAP_STEPPING;
	default:
		return TRAP_BREAKPOINT;
	}
}

/**
 * @brief Returns true if the debug event indicates that the debugged process has exited normally.
 *
 * @return true if the process exited normally, false otherwise.
 */
bool PlatformEvent::exited() const {
	return WIFEXITED(status) != 0;
}

/**
 * @brief Returns true if the debug event indicates an error condition.
 *
 * @return true if the event represents an error, false otherwise.
 */
bool PlatformEvent::is_error() const {
	if (stopped()) {
		switch (code()) {
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

/**
 * @brief Returns true if the debug event indicates that the debugged process was killed by a signal.
 *
 * @return true if the process was killed by a signal, false otherwise.
 */
bool PlatformEvent::is_kill() const {
	return stopped() && code() == SIGKILL;
}

/**
 * @brief Returns true if the debug event indicates that the debugged process was stopped.
 *
 * @return true if the process was stopped, false otherwise.
 */
bool PlatformEvent::is_stop() const {
	return stopped() && code() == SIGSTOP;
}

/**
 * @brief Returns true if the debug event indicates that the debugged process was stopped by a trap (e.g. breakpoint or single step).
 *
 * @return true if the process was stopped by a trap, false otherwise.
 */
bool PlatformEvent::is_trap() const {
	return stopped() && code() == SIGTRAP;
}

/**
 * @brief Returns true if the debug event indicates that the debugged process was terminated by a signal.
 *
 * @return true if the process was terminated by a signal, false otherwise.
 */
bool PlatformEvent::terminated() const {
	return WIFSIGNALED(status) != 0;
}

/**
 * @brief Returns true if the debug event indicates that the debugged process was stopped by a signal.
 *
 * @return true if the process was stopped by a signal, false otherwise.
 */
bool PlatformEvent::stopped() const {
	return WIFSTOPPED(status) != 0;
}

/**
 * @brief Returns the process ID for the debug event.
 *
 * @return The process ID for the debug event.
 */
edb::pid_t PlatformEvent::process() const {
	return pid;
}

/**
 * @brief Returns the thread ID for the debug event.
 *
 * @return The thread ID for the debug event.
 */
edb::tid_t PlatformEvent::thread() const {
	return tid;
}

/**
 * @brief Returns the signal code for the debug event.
 *
 * @return The signal code for the debug event.
 */
int64_t PlatformEvent::code() const {
	if (stopped()) {
		return WSTOPSIG(status);
	}

	if (terminated()) {
		return WTERMSIG(status);
	}

	if (exited()) {
		return WEXITSTATUS(status);
	}

	return 0;
}

}
