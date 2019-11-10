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

/**
 * @brief PlatformEvent::clone
 * @return
 */
PlatformEvent *PlatformEvent::clone() const {
	return new PlatformEvent(*this);
}

/**
 * @brief PlatformEvent::errorDescription
 * @return
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
 * @brief PlatformEvent::reason
 * @return
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
 * @brief PlatformEvent::trapReason
 * @return
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
 * @brief PlatformEvent::exited
 * @return
 */
bool PlatformEvent::exited() const {
	return reason() == EVENT_EXITED;
}

/**
 * @brief PlatformEvent::isError
 * @return
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
 * @brief PlatformEvent::isKill
 * @return
 */
bool PlatformEvent::isKill() const {
	return false;
}

/**
 * @brief PlatformEvent::isStop
 * @return
 */
bool PlatformEvent::isStop() const {
	return !isTrap();
}

/**
 * @brief PlatformEvent::isTrap
 * @return
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
 * @brief PlatformEvent::terminated
 * @return
 */
bool PlatformEvent::terminated() const {
	return reason() == EVENT_TERMINATED;
}

/**
 * @brief PlatformEvent::stopped
 * @return
 */
bool PlatformEvent::stopped() const {
	return reason() == EVENT_STOPPED;
}

/**
 * @brief PlatformEvent::process
 * @return
 */
edb::pid_t PlatformEvent::process() const {
	return event_.dwProcessId;
}

/**
 * @brief PlatformEvent::thread
 * @return
 */
edb::tid_t PlatformEvent::thread() const {
	return event_.dwThreadId;
}

/**
 * @brief PlatformEvent::code
 * @return
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
