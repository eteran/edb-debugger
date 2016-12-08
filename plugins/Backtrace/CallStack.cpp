/*
Copyright (C) 2015	Armen Boursalian
					aboursalian@gmail.com

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

#include "CallStack.h"
#include "edb.h"
#include "IDebugger.h"
#include "IState.h"
#include "State.h"
#include "MemoryRegions.h"
#include "Expression.h"

//TODO: This may be specific to x86... Maybe abstract this in the future.
CallStack::CallStack()
{
	get_call_stack();
}

CallStack::~CallStack() {

}

//------------------------------------------------------------------------------
// Name: get_call_stack
// Desc: Gets the state of the call stack at the time the object is created.
//------------------------------------------------------------------------------
void CallStack::get_call_stack() {
	/*
	 * Is rbp a pointer somewhere in the stack?
	 * Is the value below rbp a ret addr?
	 * Are we still scanning within the stack region?
	 */

	if(IProcess *process = edb::v1::debugger_core->process()) {
		if(IThread::pointer thread = process->current_thread()) {

			//Get the frame & stack pointers.
			State state;
			thread->get_state(&state);
			edb::address_t rbp = state.frame_pointer();
			edb::address_t rsp = state.stack_pointer();

			//Check the alignment.  rbp and rsp should be aligned to the stack.
			if (rbp % edb::v1::pointer_size() != 0 ||
					rsp % edb::v1::pointer_size() != 0)
			{
				return;
			}

			//Make sure frame pointer is pointing in the same region as stack pointer.
			//If not, then it's being used as a GPR, and we don't have enough info.
			//This assumes the stack pointer is always pointing somewhere in the stack.
			IRegion::pointer region_rsp, region_rbp;
			edb::v1::memory_regions().sync();
			region_rsp = edb::v1::memory_regions().find_region(rsp);
			region_rbp = edb::v1::memory_regions().find_region(rbp);
			if (!region_rsp || !region_rbp || (region_rbp != region_rsp) ) {
				return;
			}

			//But if we're good, then scan from rbp downward and look for return addresses.
			//Code is largely from CommentServer.cpp.  Makes assumption of size of call.
			const quint8 CALL_MIN_SIZE = 2, CALL_MAX_SIZE = 7;
			quint8 buffer[edb::Instruction::MAX_SIZE];
			for (edb::address_t addr = rbp; region_rbp->contains(addr); addr += edb::v1::pointer_size()) {

				//Get the stack value so that we can see if it's a pointer
				bool ok;
				ExpressionError err;
				edb::address_t possible_ret = edb::v1::get_value(addr, &ok, &err);

				if(IProcess *process = edb::v1::debugger_core->process()) {
					if(process->read_bytes(possible_ret - CALL_MAX_SIZE, buffer, sizeof(buffer))) {	//0xfffff... if not a ptr.
						for(int i = (CALL_MAX_SIZE - CALL_MIN_SIZE); i >= 0; --i) {
							edb::Instruction inst(buffer + i, buffer + sizeof(buffer), 0);

							//If it's a call, then make a frame
							if(is_call(inst)) {
								stack_frame frame;
								frame.ret = possible_ret;
								frame.caller = possible_ret - CALL_MAX_SIZE + i;
								stack_frames_.append(frame);
								break;
							}
						}
					}
				}
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: []
// Desc: Provides array-like access to the stack_frames_
//------------------------------------------------------------------------------
CallStack::stack_frame *CallStack::operator [](qint32 index) {
	if (index > size() || index < 0) { return 0; }
	stack_frame *frame = &(stack_frames_[index]);
	return frame;
}

//------------------------------------------------------------------------------
// Name: size
// Desc: Returns the number of frames in the call stack.
//------------------------------------------------------------------------------
int CallStack::size() {
	return stack_frames_.size();
}

//------------------------------------------------------------------------------
// Name: top
// Desc: Returns a pointer to the frame at the top of the call stack or null if
//			there are no frames on the stack
//------------------------------------------------------------------------------
CallStack::stack_frame *CallStack::top() {
	if (!size()) { return 0; }
	stack_frame *frame = &(stack_frames_.first());
	return frame;
}

//------------------------------------------------------------------------------
// Name: bottom
// Desc: Returns a pointer to the frame at the bottom of the call stack or null if
//			there are no frames on the stack
//------------------------------------------------------------------------------
CallStack::stack_frame *CallStack::bottom() {
	if (!size()) { return 0; }
	stack_frame *frame = &(stack_frames_.last());
	return frame;
}

//------------------------------------------------------------------------------
// Name: push
// Desc: Pushes a stack frame onto the top of the call stack.
//------------------------------------------------------------------------------
void CallStack::push(stack_frame frame) {
	stack_frames_.push_front(frame);
}
