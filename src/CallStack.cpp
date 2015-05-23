//LOTS OF COMMENTS AND LICENSING STUFF HERE

#include "CallStack.h"
#include "edb.h"
#include "IDebuggerCore.h"
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

void CallStack::get_call_stack() {
	/*
	 * Is rbp a pointer somewhere in the stack?
	 * Is the value below rbp a ret addr?
	 * Are we still scanning within the stack region?
	 */

	//Get the frame & stack pointers.
	State state;
	edb::v1::debugger_core->get_state(&state);
	edb::address_t rbp = state.frame_pointer();
	edb::address_t rsp = state.stack_pointer();

	//Check the alignment.  rbp and rsp should be aligned to the stack.
	if (rbp % sizeof(edb::address_t) != 0 ||
			rsp % sizeof(edb::address_t) != 0)
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
	for (edb::address_t addr = rbp; region_rbp->contains(addr); addr += sizeof(edb::address_t)) {

		//Get the stack value so that we can see if it's a pointer
		bool ok;
		ExpressionError err;
		edb::address_t possible_ret = edb::v1::get_value(addr, &ok, &err);

		if(IProcess *process = edb::v1::debugger_core->process()) {
			if(process->read_bytes(possible_ret - CALL_MAX_SIZE, buffer, sizeof(buffer))) {	//0xfffff... if not a ptr.
				for(int i = (CALL_MAX_SIZE - CALL_MIN_SIZE); i >= 0; --i) {
					edb::Instruction inst(buffer + i, buffer + sizeof(buffer), 0, std::nothrow);

					//If it's a call, then make a frame
					if(is_call(inst)) {
						stack_frame frame;
						frame.ret = possible_ret;
						frame.caller = possible_ret - CALL_MAX_SIZE + i;
						frame.invalid = false;
						stack_frames_.append(frame);
						break;
					}
				}
			}
		}
	}
}

int CallStack::size() {
	return stack_frames_.size();
}

CallStack::stack_frame CallStack::top() {
	if (!size()) {
		stack_frame f;
		return f;
	}
	return stack_frames_.first();
}

CallStack::stack_frame CallStack::bottom() {
	if (!size()) {
		stack_frame f;
		return f;
	}
	return stack_frames_.last();
}

void CallStack::push(stack_frame frame) {
	stack_frames_.push_front(frame);
}

//TODO: Is there a danger of memory leaking if the frames are not deleted?
CallStack::stack_frame CallStack::pop() {
	if (!size()) {
		stack_frame f;
		return f;
	}
	stack_frame frame = stack_frames_.first();
	stack_frames_.pop_front();
	return frame;
}
