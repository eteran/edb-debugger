
#include "PlatformThread.h"
#include "PlatformState.h"
#include "State.h"

namespace DebuggerCorePlugin {

/**
 * @brief PlatformThread::PlatformThread
 * @param core
 * @param process
 * @param hThread
 */
PlatformThread::PlatformThread(DebuggerCore *core, std::shared_ptr<IProcess> &process, HANDLE hThread) : core_(core), process_(process), handle_(hThread) {
}


/**
 * @brief PlatformThread::tid
 * @return
 */
edb::tid_t PlatformThread::tid() const {
	return GetThreadId(handle_);
}

/**
 * @brief PlatformThread::name
 * @return
 */
QString PlatformThread::name() const {

	WCHAR *data;
	HRESULT hr = GetThreadDescription(handle_, &data);
	if (SUCCEEDED(hr)) {
		auto name = QString::fromWCharArray(data);
		LocalFree(data);
		return name;
	}

	return tr("Thread: %1").arg(tid());

}

/**
 * @brief PlatformThread::priority
 * @return
 */
int PlatformThread::priority() const {
	return GetThreadPriority(handle_);
}

/**
 * @brief PlatformThread::instruction_pointer
 * @return
 */
edb::address_t PlatformThread::instruction_pointer() const {
	return {};
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
void PlatformThread::get_state(State *state) {
	if(auto p = static_cast<PlatformState *>(state->impl_.get())) {
		p->context_.ContextFlags = CONTEXT_ALL; //CONTEXT_FULL | CONTEXT_DEBUG_REGISTERS | CONTEXT_FLOATING_POINT;
		GetThreadContext(handle_, &p->context_);

		p->gs_base_ = 0;
		p->fs_base_ = 0;
		// GetThreadSelectorEntry always returns false on x64
		// on x64 gs_base == TEB, maybe we can use that somehow
#if !defined(EDB_X86_64) || 1
		LDT_ENTRY ldt_entry;
		if(GetThreadSelectorEntry(handle_, p->context_.SegGs, &ldt_entry)) {
			p->gs_base_ = ldt_entry.BaseLow | (ldt_entry.HighWord.Bits.BaseMid << 16) | (ldt_entry.HighWord.Bits.BaseHi << 24);
		}

		if(GetThreadSelectorEntry(handle_, p->context_.SegFs, &ldt_entry)) {
			p->fs_base_ = ldt_entry.BaseLow | (ldt_entry.HighWord.Bits.BaseMid << 16) | (ldt_entry.HighWord.Bits.BaseHi << 24);
		}
#endif
	}
}

/**
 * @brief PlatformThread::set_state
 * @param state
 */
void PlatformThread::set_state(const State &state) {
	if(auto p = static_cast<const PlatformState *>(state.impl_.get())) {
#if 0
		p->context_.ContextFlags = CONTEXT_ALL; //CONTEXT_FULL | CONTEXT_DEBUG_REGISTERS | CONTEXT_FLOATING_POINT;
#endif
		SetThreadContext(handle_, &p->context_);
	}
}

/**
 * @brief PlatformThread::step
 * @return
 */
Status PlatformThread::step() {

	CONTEXT context;
	context.ContextFlags = CONTEXT_CONTROL;
	GetThreadContext(handle_, &context);
	context.EFlags |= (1 << 8); // set the trap flag
	SetThreadContext(handle_, &context);

	return resume();
}

/**
 * @brief PlatformThread::step
 * @param status
 * @return
 */
Status PlatformThread::step(edb::EVENT_STATUS status) {
	CONTEXT context;
	context.ContextFlags = CONTEXT_CONTROL;
	GetThreadContext(handle_, &context);
	context.EFlags |= (1 << 8); // set the trap flag
	SetThreadContext(handle_, &context);

	return resume(status);
}

/**
 * @brief PlatformThread::resume
 * @return
 */
Status PlatformThread::resume() {
	ContinueDebugEvent(process_->pid(), tid(), DBG_CONTINUE);
	return Status::Ok;
}

/**
 * @brief PlatformThread::resume
 * @param status
 * @return
 */
Status PlatformThread::resume(edb::EVENT_STATUS status) {
	ContinueDebugEvent(
	    process_->pid(),
	    tid(),
	    (status == edb::DEBUG_CONTINUE) ? (DBG_CONTINUE) : (DBG_EXCEPTION_NOT_HANDLED));
	return Status::Ok;
}

/**
 * @brief PlatformThread::stop
 * @return
 */
Status PlatformThread::stop() {
	SuspendThread(handle_);
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
