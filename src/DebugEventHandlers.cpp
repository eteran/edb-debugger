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

#include "DebugEventHandlers.h"
#include "Util.h"
#include "IDebugEventHandler.h"

DebugEventHandlers::~DebugEventHandlers() {
	if (current_handler_) {
		EDB_PRINT_AND_DIE("can not destroy debug events container while executing events");
	}

	if (!handlers_.empty()) {
		EDB_PRINT_AND_DIE("some debug event handlers weren't property removed");
	}
}

void DebugEventHandlers::add(IDebugEventHandler *handler) {
	if (!handler) {
		EDB_PRINT_AND_DIE("event handler can not be nullptr");
	}

	auto it = std::find(handlers_.begin(), handlers_.end(), handler);
	if (it != handlers_.end()) {
		EDB_PRINT_AND_DIE("the same event handler is added twice");
	}

	// DebugEventHandlers::add can be called inside DebugEventHandlers::execute.
	// Let's insert the handler in front so it is not called on the
	// event currently being executed
	handlers_.push_front(handler);
}

void DebugEventHandlers::remove(IDebugEventHandler *handler) {
	if (!handler) {
		EDB_PRINT_AND_DIE("event handler can not be nullptr");
	}

	auto it = std::find(handlers_.begin(), handlers_.end(), handler);
	if (it == handlers_.end()) {
		EDB_PRINT_AND_DIE("removal of an event that is not present");
	}

	// during execution only deletion of the current handler is supported
	if (current_handler_ && current_handler_ != handler) {
		EDB_PRINT_AND_DIE("removal of non-current event handler during execution");
	}

	handlers_.erase(it);
}

edb::EVENT_STATUS DebugEventHandlers::execute(const std::shared_ptr<IDebugEvent> &event) {
	if (current_handler_) {
		EDB_PRINT_AND_DIE("recursive debug event execution is not allowed");
	}

	// if somehow no handler is run, then let's just assume we should stop...
	edb::EVENT_STATUS status = edb::DEBUG_STOP;

	try {
		// loop through all of the handlers, stopping when one thinks that it handled
		// the event
		for (auto it = handlers_.begin(), end = handlers_.end(); it != end;) {
			// increment before processing, so if it's deleted it's not a problem
			current_handler_ = *it++;
			status = current_handler_->handle_event(event);
			if (status != edb::DEBUG_NEXT_HANDLER) {
				break;
			}
		}
	} catch (...) {
		// reset current_handler_ to nullptr even if an exception was thrown
		current_handler_ = nullptr;
		throw;
	}

	current_handler_ = nullptr;

	// if this assert fails, the bottom handler (which is owned by us) did something terribly wrong :-/
	if (status == edb::DEBUG_NEXT_HANDLER) {
		EDB_PRINT_AND_DIE("the last event handler returned DEBUG_NEXT_HANDLER");
	}

	return status;
}
