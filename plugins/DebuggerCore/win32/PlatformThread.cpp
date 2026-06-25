/*
 * Copyright (C) 2015 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "PlatformThread.h"
#include "PlatformProcess.h"
#include "PlatformState.h"
#include "State.h"

namespace DebuggerCorePlugin {

/**
 * @brief Creates a new PlatformThread instance with the specified parameters.
 *
 * @param core The DebuggerCore instance associated with this thread.
 * @param process The process associated with this thread.
 * @param hThread The handle to the thread.
 */
PlatformThread::PlatformThread(DebuggerCore *core, std::shared_ptr<IProcess> &process, const CREATE_THREAD_DEBUG_INFO *info)
	: core_(core), process_(process) {

	isWow64_ = static_cast<PlatformProcess *>(process.get())->isWow64();

	DuplicateHandle(
		GetCurrentProcess(),
		info->hThread,
		GetCurrentProcess(),
		&hThread_,
		0,
		FALSE,
		DUPLICATE_SAME_ACCESS);
}

/**
 * @brief Gets the thread ID of the current PlatformThread instance.
 *
 * @return The thread ID of the thread.
 */
edb::tid_t PlatformThread::tid() const {
	return GetThreadId(hThread_);
}

/**
 * @brief Gets the name of the current PlatformThread instance.
 *
 * @return The name of the thread.
 */
QString PlatformThread::name() const {

	using GetThreadDescriptionType = HRESULT(WINAPI *)(HANDLE, PWSTR *);

	static auto fnGetThreadDescription = reinterpret_cast<GetThreadDescriptionType>(GetProcAddress(GetModuleHandle(TEXT("kernel32")), "GetThreadDescription"));
	if (fnGetThreadDescription) {
		WCHAR *data;
		HRESULT hr = fnGetThreadDescription(hThread_, &data);
		if (SUCCEEDED(hr)) {
			auto name = QString::fromWCharArray(data);
			LocalFree(data);
			return name;
		}
	}

	return tr("Thread: %1").arg(tid());
}

/**
 * @brief Gets the priority of the current PlatformThread instance.
 *
 * @return The priority of the thread.
 */
int PlatformThread::priority() const {
	return GetThreadPriority(hThread_);
}

/**
 * @brief Gets the instruction pointer of the current PlatformThread instance.
 *
 * @return The instruction pointer of the thread.
 */
edb::address_t PlatformThread::instructionPointer() const {
#if defined(EDB_X86)
	CONTEXT context;
	context.ContextFlags = CONTEXT_CONTROL;
	GetThreadContext(hThread_, &context);
	return context.Eip;
#elif defined(EDB_X86_64)
	if (isWow64_) {
		WOW64_CONTEXT context;
		context.ContextFlags = CONTEXT_CONTROL;
		Wow64GetThreadContext(hThread_, &context);
		return context.Eip;
	} else {
		CONTEXT context;
		context.ContextFlags = CONTEXT_CONTROL;
		GetThreadContext(hThread_, &context);
		return context.Rip;
	}
#endif
}

/**
 * @brief Gets the run state of the current PlatformThread instance.
 *
 * @return The run state of the thread.
 */
QString PlatformThread::runState() const {
	return {};
}

/**
 * @brief Gets the state of the current PlatformThread instance.
 *
 * @param state A pointer to a State object where the thread state will be stored.
 */
void PlatformThread::getState(State *state) {
	if (auto p = static_cast<PlatformState *>(state->impl_.get())) {
		p->getThreadState(hThread_, isWow64_);
	}
}

/**
 * @brief Sets the state of the current PlatformThread instance.
 *
 * @param state A reference to a State object containing the new thread state to be set.
 */
void PlatformThread::setState(const State &state) {
	if (auto p = static_cast<const PlatformState *>(state.impl_.get())) {
		p->setThreadState(hThread_);
	}
}

/**
 * @brief Steps the current PlatformThread instance, executing a single instruction.
 *
 * @return A Status object indicating the success or failure of the step operation.
 */
Status PlatformThread::step() {
#if defined(EDB_X86)
	CONTEXT context;
	context.ContextFlags = CONTEXT_CONTROL;
	GetThreadContext(hThread_, &context);
	context.EFlags |= (1 << 8); // set the trap flag
	SetThreadContext(hThread_, &context);
#elif defined(EDB_X86_64)
	if (isWow64_) {
		WOW64_CONTEXT context;
		context.ContextFlags = CONTEXT_CONTROL;
		Wow64GetThreadContext(hThread_, &context);
		context.EFlags |= (1 << 8); // set the trap flag
		Wow64SetThreadContext(hThread_, &context);
	} else {
		CONTEXT context;
		context.ContextFlags = CONTEXT_CONTROL;
		GetThreadContext(hThread_, &context);
		context.EFlags |= (1 << 8); // set the trap flag
		SetThreadContext(hThread_, &context);
	}
#endif

	return resume();
}

/**
 * @brief Steps the current PlatformThread instance, executing a single instruction with the specified event status.
 *
 * @param status The event status to be used during the step operation.
 * @return A Status object indicating the success or failure of the step operation.
 */
Status PlatformThread::step(edb::EventStatus status) {
#if defined(EDB_X86)
	CONTEXT context;
	context.ContextFlags = CONTEXT_CONTROL;
	GetThreadContext(hThread_, &context);
	context.EFlags |= (1 << 8); // set the trap flag
	SetThreadContext(hThread_, &context);
#elif defined(EDB_X86_64)
	if (isWow64_) {
		WOW64_CONTEXT context;
		context.ContextFlags = CONTEXT_CONTROL;
		Wow64GetThreadContext(hThread_, &context);
		context.EFlags |= (1 << 8); // set the trap flag
		Wow64SetThreadContext(hThread_, &context);
	} else {
		CONTEXT context;
		context.ContextFlags = CONTEXT_CONTROL;
		GetThreadContext(hThread_, &context);
		context.EFlags |= (1 << 8); // set the trap flag
		SetThreadContext(hThread_, &context);
	}
#endif

	return resume(status);
}

/**
 * @brief Gets whether the current PlatformThread instance is paused.
 *
 * @return A Status object indicating the success or failure of the resume operation.
 */
Status PlatformThread::resume() {

	// TODO(eteran): suspend the other threads, then basically just call process_->resume

	ContinueDebugEvent(process_->pid(), tid(), DBG_CONTINUE);
	return Status::Ok;
}

/**
 * @brief Gets whether the current PlatformThread instance is paused with the specified event status.
 *
 * @param status The event status to be used during the resume operation.
 * @return A Status object indicating the success or failure of the resume operation.
 */
Status PlatformThread::resume(edb::EventStatus status) {

	// TODO(eteran): suspend the other threads, then basically just call process_->resume

	ContinueDebugEvent(
		process_->pid(),
		tid(),
		(status == edb::DEBUG_CONTINUE) ? (DBG_CONTINUE) : (DBG_EXCEPTION_NOT_HANDLED));
	return Status::Ok;
}

/**
 * @brief Gets whether the current PlatformThread instance is paused.
 *
 * @return true if the thread is paused, false otherwise.
 */
bool PlatformThread::isPaused() const {
	return {};
}

}
