/*
Copyright (C) 2018 Ivan Sorokin
                   vanyacpp@gmail.com

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

#ifndef DEBUG_EVENT_HANDLERS_H_20180312_
#define DEBUG_EVENT_HANDLERS_H_20180312_

#include "Types.h"
#include <list>
#include <memory>

class IDebugEventHandler;
class IDebugEvent;

/*
This class is a container of IDebugEventHandler*. It supports three
operations:
1. add -- Adding IDebugEventHandler*
2. remove -- Removing IDebugEventHandler*
3. execute -- Iterating over added handlers and calling them

The reason why a simple standard container is not enough is that
handlers can be added/removed while the list is being iterated on.
Naive implement would cause accessing an invalid iterator in this
case.

This class supports adding new handlers during iteration. Removing
during iteration is also supported, but is restricted: only the
handler that is currectly being executed can be removed.

In order to support removing handlers during iteration the iterator
is incremented before calling corresponding handler.

Recursive calls to execute are forbidden.

Contract violations of this class are unrecoverable and cause
program termination (std::abort is called).
*/
class DebugEventHandlers {
public:
	DebugEventHandlers()                           = default;
	DebugEventHandlers(const DebugEventHandlers &) = delete;
	DebugEventHandlers &operator=(const DebugEventHandlers &) = delete;
	~DebugEventHandlers();

	void add(IDebugEventHandler *handler);
	void remove(IDebugEventHandler *handler);
	edb::EventStatus execute(const std::shared_ptr<IDebugEvent> &event);

private:
	/*
	list of registed handlers
	*/
	std::list<IDebugEventHandler *> handlers_;

	/*
	the current handler that is being executed
	nullptr means that none is being executed at the moment
	*/
	IDebugEventHandler *currentHandler_ = nullptr;
};

#endif
