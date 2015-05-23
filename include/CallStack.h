#ifndef CALLSTACK_H
#define CALLSTACK_H

#include "edb.h"

#include <QList>
#include <QtCore/qglobal.h>

class CallStack
{
public:
	CallStack();
	~CallStack();

public:
	typedef struct stack_frame_t {
		edb::address_t ret;
		edb::address_t caller;
		bool invalid = true;
	} stack_frame;

public:
	void get_call_stack();
	int size();
	stack_frame top();
	stack_frame bottom();
	void push(stack_frame frame);
	stack_frame pop();

private:
	QList<stack_frame> stack_frames_;
};

#endif // CALLSTACK_H
