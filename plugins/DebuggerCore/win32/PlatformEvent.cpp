/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "PlatformEvent.h"
#include "edb.h"

namespace DebuggerCorePlugin {

/**
 * @brief Clones the current PlatformEvent instance.
 *
 * @return A pointer to a new PlatformEvent instance that is a copy of the current instance.
 */
PlatformEvent *PlatformEvent::clone() const {
	return new PlatformEvent(*this);
}

/**
 * @brief Gets the error description for the current PlatformEvent instance.
 *
 * @return An IDebugEvent::Message object containing the error description.
 */
IDebugEvent::Message PlatformEvent::errorDescription() const {
	Q_ASSERT(isError());

	auto fault_address = static_cast<edb::address_t>(-1);
	if (event_.dwDebugEventCode == EXCEPTION_DEBUG_EVENT) {
		fault_address = static_cast<edb::address_t>(event_.u.Exception.ExceptionRecord.ExceptionInformation[1]);
	}

	switch (code()) {
	case EXCEPTION_ACCESS_VIOLATION:
		return Message(
			tr("Illegal Access Fault"),
			tr(
				"<p>The debugged application encountered a segmentation fault.<br />The address <strong>0x%1</strong> could not be accessed.</p>"
				"<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>")
				.arg(edb::v1::format_pointer(fault_address)),
			tr("EXCEPTION_ACCESS_VIOLATION"));
	case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
		return Message(
			tr("Array Bounds Error"),
			tr(
				"<p>The debugged application tried to access an out of bounds array element.</p>"
				"<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>")
				.arg(edb::v1::format_pointer(fault_address)),
			tr("EXCEPTION_ARRAY_BOUNDS_EXCEEDED"));
	case EXCEPTION_DATATYPE_MISALIGNMENT:
		return Message(
			tr("Bus Error"),
			tr(
				"<p>The debugged application tried to read or write data that is misaligned.</p>"
				"<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>"),
			tr("EXCEPTION_DATATYPE_MISALIGNMENT"));
	case EXCEPTION_FLT_DENORMAL_OPERAND:
		return Message(
			tr("Floating Point Exception"),
			tr(
				"<p>One of the operands in a floating-point operation is denormal. A denormal value is one that is too small to represent as a standard floating-point value.</p>"
				"<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>"),
			tr("EXCEPTION_FLT_DENORMAL_OPERAND"));

	case EXCEPTION_FLT_DIVIDE_BY_ZERO:
		return Message(
			tr("Floating Point Exception"),
			tr(
				"<p>The debugged application tried to divide a floating-point value by a floating-point divisor of zero.</p>"
				"<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>"),
			tr("EXCEPTION_FLT_DIVIDE_BY_ZERO"));
	case EXCEPTION_FLT_INEXACT_RESULT:
		return Message(
			tr("Floating Point Exception"),
			tr(
				"<p>The result of a floating-point operation cannot be represented exactly as a decimal fraction.</p>"
				"<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>"),
			tr("EXCEPITION_FLT_INEXACT_RESULT"));
	case EXCEPTION_FLT_INVALID_OPERATION:
		return Message(
			tr("Floating Point Exception"),
			tr(
				"<p>The application attempted an invalid floating point operation.</p>"
				"<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>"),
			tr("EXCEPTION_FLT_INVALID_OPERATION"));
	case EXCEPTION_FLT_OVERFLOW:
		return Message(
			tr("Floating Point Exception"),
			tr(
				"<p>The exponent of a floating-point operation is greater than the magnitude allowed by the corresponding type.</p>"
				"<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>"),
			tr("EXCEPTION_FLT_OVERFLOW"));
	case EXCEPTION_FLT_STACK_CHECK:
		return Message(
			tr("Floating Point Exception"),
			tr(
				"<p>The stack overflowed or underflowed as the result of a floating-point operation.</p>"
				"<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>"),
			tr("EXCEPTION_FLT_STACK_CHECK"));
	case EXCEPTION_FLT_UNDERFLOW:
		return Message(
			tr("Floating Point Exception"),
			tr(
				"<p>The exponent of a floating-point operation is less than the magnitude allowed by the corresponding type.</p>"
				"<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>"),
			tr("EXCEPTION_FLT_UNDERFLOW"));
	case EXCEPTION_ILLEGAL_INSTRUCTION:
		return Message(
			tr("Illegal Instruction Fault"),
			tr(
				"<p>The debugged application attempted to execute an illegal instruction.</p>"
				"<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>"),
			tr("EXCEPTION_ILLEGAL_INSTRUCTION"));
	case EXCEPTION_IN_PAGE_ERROR:
		return Message(
			tr("Page Error"),
			tr(
				"<p>The debugged application tried to access a page that was not present, and the system was unable to load the page.</p>"
				"<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>"),
			tr("EXCEPTION_IN_PAGE_ERROR"));
	case EXCEPTION_INT_DIVIDE_BY_ZERO:
		return Message(
			tr("Divide By Zero"),
			tr(
				"<p>The debugged application tried to divide an integer value by an integer divisor of zero.</p>"
				"<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>"),
			tr("EXCEPTION_INT_DIVIDE_BY_ZERO"));
	case EXCEPTION_INT_OVERFLOW:
		return Message(
			tr("Integer Overflow"),
			tr(
				"<p>The result of an integer operation caused a carry out of the most significant bit of the result.</p>"
				"<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>"),
			tr("EXCEPTION_INT_OVERFLOW"));
	case EXCEPTION_INVALID_DISPOSITION:
		return Message(
			tr("Invalid Disposition"),
			tr(
				"<p>An exception handler returned an invalid disposition to the exception dispatcher.</p>"
				"<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>"),
			tr("EXCEPTION_INVALID_DISPOSITION"));
	case EXCEPTION_NONCONTINUABLE_EXCEPTION:
		return Message(
			tr("Non-Continuable Exception"),
			tr(
				"<p>The debugged application tried to continue execution after a non-continuable exception occurred.</p>"
				"<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>"),
			tr("EXCEPTION_NONCONTINUABLE_EXCEPTION"));
	case EXCEPTION_PRIV_INSTRUCTION:
		return Message(
			tr("Privileged Instruction"),
			tr(
				"<p>The debugged application tried to execute an instruction whose operation is not allowed in the current machine mode.</p>"
				"<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>"),
			tr("EXCEPTION_PRIV_INSTRUCTION"));
	case EXCEPTION_STACK_OVERFLOW:
		return Message(
			tr("Stack Overflow"),
			tr(
				"<p>The debugged application has exhausted its stack.</p>"
				"<p>If you would like to pass this exception to the application press Shift+[F7/F8/F9]</p>"),
			tr("EXCEPTION_STACK_OVERFLOW"));
	default:
		return Message();
	}
}

/**
 * @brief Gets the reason for the current PlatformEvent instance.
 *
 * @return A value indicating the reason for the event.
 */
IDebugEvent::REASON PlatformEvent::reason() const {
	switch (event_.dwDebugEventCode) {
	case EXCEPTION_DEBUG_EVENT:
		if (event_.u.Exception.ExceptionRecord.ExceptionFlags == EXCEPTION_NONCONTINUABLE) {
			return EVENT_TERMINATED;
		} else {
			return EVENT_STOPPED;
		}
	case EXIT_PROCESS_DEBUG_EVENT:
		return EVENT_EXITED;
	/*
	case CREATE_THREAD_DEBUG_EVENT:
	case CREATE_PROCESS_DEBUG_EVENT:
	case EXIT_THREAD_DEBUG_EVENT:
	case LOAD_DLL_DEBUG_EVENT:
	case UNLOAD_DLL_DEBUG_EVENT:
	case OUTPUT_DEBUG_STRING_EVENT:
	case RIP_EVENT:
	*/
	default:
		return EVENT_UNKNOWN;
	}
}

/**
 * @brief Gets the trap reason for the current PlatformEvent instance.
 *
 * @return A value indicating the reason for the trap.
 */
IDebugEvent::TRAP_REASON PlatformEvent::trapReason() const {
	switch (event_.dwDebugEventCode) {
	case EXCEPTION_DEBUG_EVENT:
		switch (event_.u.Exception.ExceptionRecord.ExceptionCode) {
		case EXCEPTION_BREAKPOINT:
			return TRAP_BREAKPOINT;
		case EXCEPTION_SINGLE_STEP:
			return TRAP_STEPPING;
		}
	}
	return TRAP_BREAKPOINT;
}

/**
 * @brief Gets whether the debugged process has exited.
 *
 * @return true if the process has exited, false otherwise.
 */
bool PlatformEvent::exited() const {
	return reason() == EVENT_EXITED;
}

/**
 * @brief Gets whether the current PlatformEvent instance represents an error.
 *
 * @return true if the event represents an error, false otherwise.
 */
bool PlatformEvent::isError() const {
	switch (event_.dwDebugEventCode) {
	case EXCEPTION_DEBUG_EVENT:
		switch (event_.u.Exception.ExceptionRecord.ExceptionCode) {
		case EXCEPTION_ACCESS_VIOLATION:
		case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
		case EXCEPTION_DATATYPE_MISALIGNMENT:
		case EXCEPTION_FLT_DENORMAL_OPERAND:
		case EXCEPTION_FLT_DIVIDE_BY_ZERO:
		case EXCEPTION_FLT_INEXACT_RESULT:
		case EXCEPTION_FLT_INVALID_OPERATION:
		case EXCEPTION_FLT_OVERFLOW:
		case EXCEPTION_FLT_STACK_CHECK:
		case EXCEPTION_FLT_UNDERFLOW:
		case EXCEPTION_ILLEGAL_INSTRUCTION:
		case EXCEPTION_INT_DIVIDE_BY_ZERO:
		case EXCEPTION_INT_OVERFLOW:
		case EXCEPTION_INVALID_DISPOSITION:
		case EXCEPTION_IN_PAGE_ERROR:
		case EXCEPTION_NONCONTINUABLE_EXCEPTION:
		case EXCEPTION_PRIV_INSTRUCTION:
		case EXCEPTION_STACK_OVERFLOW:
			return true;
		case EXCEPTION_BREAKPOINT:
		case EXCEPTION_SINGLE_STEP:
			return false;
		}
	/*
	case CREATE_THREAD_DEBUG_EVENT:
	case CREATE_PROCESS_DEBUG_EVENT:
	case EXIT_THREAD_DEBUG_EVENT:
	case EXIT_PROCESS_DEBUG_EVENT:
	case LOAD_DLL_DEBUG_EVENT:
	case UNLOAD_DLL_DEBUG_EVENT:
	case OUTPUT_DEBUG_STRING_EVENT:
	case RIP_EVENT:
	*/
	default:
		return false;
	}
}

/**
 * @brief Gets whether the current PlatformEvent instance represents a kill event.
 *
 * @return true if the event represents a kill event, false otherwise.
 */
bool PlatformEvent::isKill() const {
	return false;
}

/**
 * @brief Gets whether the current PlatformEvent instance represents a stop event.
 *
 * @return true if the event represents a stop event, false otherwise.
 */
bool PlatformEvent::isStop() const {
	return !isTrap();
}

/**
 * @brief Gets whether the current PlatformEvent instance represents a trap event.
 *
 * @return true if the event represents a trap event, false otherwise.
 */
bool PlatformEvent::isTrap() const {
	if (stopped()) {
		switch (event_.u.Exception.ExceptionRecord.ExceptionCode) {
		case EXCEPTION_SINGLE_STEP:
		case EXCEPTION_BREAKPOINT:
			return true;
		default:
			return false;
		}
	}
	return false;
}

/**
 * @brief Gets whether the current PlatformEvent instance represents a terminated event.
 *
 * @return true if the event represents a terminated event, false otherwise.
 */
bool PlatformEvent::terminated() const {
	return reason() == EVENT_TERMINATED;
}

/**
 * @brief Gets whether the current PlatformEvent instance represents a stopped event.
 *
 * @return true if the event represents a stopped event, false otherwise.
 */
bool PlatformEvent::stopped() const {
	return reason() == EVENT_STOPPED;
}

/**
 * @brief Gets the process ID associated with the current PlatformEvent instance.
 *
 * @return the process ID.
 */
edb::pid_t PlatformEvent::process() const {
	return event_.dwProcessId;
}

/**
 * @brief Gets the thread ID associated with the current PlatformEvent instance.
 *
 * @return the thread ID.
 */
edb::tid_t PlatformEvent::thread() const {
	return event_.dwThreadId;
}

/**
 * @brief Gets the exception code associated with the current PlatformEvent instance.
 *
 * @return the exception code.
 */
int64_t PlatformEvent::code() const {
	if (stopped()) {
		return event_.u.Exception.ExceptionRecord.ExceptionCode;
	}

	if (terminated()) {
		return event_.u.Exception.ExceptionRecord.ExceptionCode;
	}

	if (exited()) {
		return event_.u.ExitProcess.dwExitCode;
	}

	return 0;
}

}
