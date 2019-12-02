/*
Copyright (C) 2015 - 2018 Evan Teran
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
#include "PlatformProcess.h"
#include "PlatformState.h"
#include "State.h"

namespace DebuggerCorePlugin {

/**
 * @brief PlatformThread::PlatformThread
 * @param core
 * @param process
 * @param hThread
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
 * @brief PlatformThread::tid
 * @return
 */
edb::tid_t PlatformThread::tid() const {
	return GetThreadId(hThread_);
}

/**
 * @brief PlatformThread::name
 * @return
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
 * @brief PlatformThread::priority
 * @return
 */
int PlatformThread::priority() const {
	return GetThreadPriority(hThread_);
}

/**
 * @brief PlatformThread::instructionPointer
 * @return
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
 * @brief PlatformThread::runState
 * @return
 */
QString PlatformThread::runState() const {
	return {};
}

/**
 * @brief PlatformThread::get_state
 * @param state
 */
void PlatformThread::getState(State *state) {
	if (auto p = static_cast<PlatformState *>(state->impl_.get())) {
		p->getThreadState(hThread_, isWow64_);
	}
}

/**
 * @brief PlatformThread::set_state
 * @param state
 */
void PlatformThread::setState(const State &state) {
	if (auto p = static_cast<const PlatformState *>(state.impl_.get())) {
		p->setThreadState(hThread_);
	}
}

/**
 * @brief PlatformThread::step
 * @return
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
 * @brief PlatformThread::step
 * @param status
 * @return
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
 * @brief PlatformThread::resume
 * @return
 */
Status PlatformThread::resume() {

	// TODO(eteran): suspend the other threads, then basically just call process_->resume

	ContinueDebugEvent(process_->pid(), tid(), DBG_CONTINUE);
	return Status::Ok;
}

/**
 * @brief PlatformThread::resume
 * @param status
 * @return
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
 * @brief PlatformThread::isPaused
 * @return
 */
bool PlatformThread::isPaused() const {
	return {};
}

}
