
#include "PlatformThread.h"
#include "PlatformState.h"
#include "State.h"

namespace DebuggerCorePlugin {

/**
 * @brief PlatformThread::PlatformThread
 * @param core
 * @param process
 * @param tid
 */
PlatformThread::PlatformThread(DebuggerCore *core, std::shared_ptr<IProcess> &process, edb::tid_t tid, CREATE_THREAD_DEBUG_INFO *CreateThread) : core_(core), process_(process), tid_(tid) {
	info_ = *CreateThread;
	handle_ = OpenThread(THREAD_QUERY_INFORMATION | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, tid);
}

/**
 * @brief PlatformThread::~PlatformThread
 */
PlatformThread::~PlatformThread() {
	if(handle_) {
		CloseHandle(handle_);
	}
}


/**
 * @brief PlatformThread::tid
 * @return
 */
edb::tid_t PlatformThread::tid() const {
	return tid_;
}

/**
 * @brief PlatformThread::name
 * @return
 */
QString PlatformThread::name() const {
	return tr("Thread: %1").arg(tid_);
}

/**
 * @brief PlatformThread::priority
 * @return
 */
int PlatformThread::priority() const {
	return GetThreadPriority(info_.hThread);
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
		GetThreadContext(info_.hThread, &p->context_);
	}
}

/**
 * @brief PlatformThread::set_state
 * @param state
 */
void PlatformThread::set_state(const State &state) {
	if(auto p = static_cast<const PlatformState *>(state.impl_.get())) {
		SetThreadContext(info_.hThread, &p->context_);
	}
}

/**
 * @brief PlatformThread::step
 * @return
 */
Status PlatformThread::step() {
	return Status::Ok;
}

/**
 * @brief PlatformThread::step
 * @param status
 * @return
 */
Status PlatformThread::step(edb::EVENT_STATUS status) {
	return Status::Ok;
}

/**
 * @brief PlatformThread::resume
 * @return
 */
Status PlatformThread::resume() {
	return Status::Ok;
}

/**
 * @brief PlatformThread::resume
 * @param status
 * @return
 */
Status PlatformThread::resume(edb::EVENT_STATUS status) {
	return Status::Ok;
}

/**
 * @brief PlatformThread::stop
 * @return
 */
Status PlatformThread::stop() {
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
