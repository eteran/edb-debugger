/*
 * Copyright (C) 2015 Armen Boursalian <aboursalian@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "CallStack.h"
#include "Expression.h"
#include "IDebugger.h"
#include "IProcess.h"
#include "IRegion.h"
#include "IState.h"
#include "IThread.h"
#include "MemoryRegions.h"
#include "State.h"
#include "edb.h"

// TODO: This may be specific to x86... Maybe abstract this in the future.

/**
 * @brief Constructs a CallStack by immediately snapshotting the current call stack state.
 */
CallStack::CallStack() {
	getCallStack();
}

/**
 * @brief Walks the stack frame chain from the current frame pointer to build the list of return addresses.
 *
 */
void CallStack::getCallStack() {
	/*
	 * Is rbp a pointer somewhere in the stack?
	 * Is the value below rbp a ret addr?
	 * Are we still scanning within the stack region?
	 */

	if (IProcess *process = edb::v1::debugger_core->process()) {
		if (std::shared_ptr<IThread> thread = process->currentThread()) {

			// Get the frame & stack pointers.
			State state;
			thread->getState(&state);
			const edb::address_t rbp = state.framePointer();
			const edb::address_t rsp = state.stackPointer();

			// Check the alignment.  rbp and rsp should be aligned to the stack.
			if (rbp % edb::v1::pointer_size() != 0 || rsp % edb::v1::pointer_size() != 0) {
				qDebug("It appears that the application is not using frame pointers, call stack unavailable.");
				return;
			}

			// Make sure frame pointer is pointing in the same region as stack pointer.
			// If not, then it's being used as a GPR, and we don't have enough info.
			// This assumes the stack pointer is always pointing somewhere in the stack.
			edb::v1::memory_regions().sync();
			std::shared_ptr<IRegion> region_rsp = edb::v1::memory_regions().findRegion(rsp);
			std::shared_ptr<IRegion> region_rbp = edb::v1::memory_regions().findRegion(rbp);
			if (!region_rsp || !region_rbp || (region_rbp != region_rsp)) {
				return;
			}

			// But if we're good, then scan from rbp downward and look for return addresses.
			// Code is largely from CommentServer.cpp.  Makes assumption of size of call.
			constexpr uint8_t CallMinSize = 2;
			constexpr uint8_t CallMaxSize = 7;

			uint8_t buffer[edb::Instruction::MaxSize];
			for (edb::address_t addr = rbp; region_rbp->contains(addr); addr += edb::v1::pointer_size()) {

				// Get the stack value so that we can see if it's a pointer
				bool ok;
				ExpressionError err;
				edb::address_t possible_ret = edb::v1::get_value(addr, &ok, &err);

				if (process->readBytes(possible_ret - CallMaxSize, buffer, sizeof(buffer))) { // 0xfffff... if not a ptr.
					for (int i = (CallMaxSize - CallMinSize); i >= 0; --i) {
						edb::Instruction inst(buffer + i, buffer + sizeof(buffer), 0);

						// If it's a call, then make a frame
						if (is_call(inst)) {
							StackFrame frame;
							frame.ret    = possible_ret;
							frame.caller = possible_ret - CallMaxSize + i;
							stackFrames_.push_back(frame);
							break;
						}
					}
				}
			}
		}
	}
}

/**
 * @brief Returns a pointer to the stack frame at the given index, or nullptr if the index is out of range.
 *
 * @param index
 * @return
 */
CallStack::StackFrame *CallStack::operator[](size_t index) {
	if (index > size()) {
		return nullptr;
	}

	return &stackFrames_[index];
}

/**
 * @brief Returns the number of frames captured in the call stack.
 * @return the number of frames in the call stack.
 */
size_t CallStack::size() const {
	return stackFrames_.size();
}

/**
 * @brief Returns a pointer to the topmost (most recent) stack frame, or nullptr if the stack is empty.
 * @return a pointer to the frame at the top of the call stack or nullptr
 * if there are no frames on the stack
 */
CallStack::StackFrame *CallStack::top() {
	if (!size()) {
		return nullptr;
	}

	return &stackFrames_.front();
}

/**
 * @brief Returns a pointer to the bottommost (oldest) stack frame, or nullptr if the stack is empty.
 * @return a pointer to the frame at the bottom of the call stack or nullptr
 * if there are no frames on the stack
 */
CallStack::StackFrame *CallStack::bottom() {
	if (!size()) {
		return nullptr;
	}

	return &stackFrames_.back();
}

/**
 * @brief Pushes a new stack frame onto the top of the call stack.
 *
 * @param frame
 */
void CallStack::push(StackFrame frame) {
	stackFrames_.push_front(frame);
}
