/*
Copyright (C) 2006 - 2011 Evan Teran
                          eteran@alum.rit.edu

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

#ifndef DEBUGEVENT_20121005_H_
#define DEBUGEVENT_20121005_H_

#include "API.h"
#include "IDebugEvent.h"

class EDB_EXPORT DebugEvent {
	friend class DebuggerCore;
	
public:
	DebugEvent();
	~DebugEvent();
	
public:
	DebugEvent(const DebugEvent &other);
	DebugEvent &operator=(const DebugEvent &rhs);
	
public:
	void swap(DebugEvent &other);
	operator void *() const { return reinterpret_cast<void *>(impl_ != 0); }
	
public:
	IDebugEvent::Message error_description() const;
	IDebugEvent::REASON reason() const;
	IDebugEvent::TRAP_REASON trap_reason() const;
	bool exited() const;
	bool is_error() const;
	bool is_kill() const;
	bool is_stop() const;
	bool is_trap() const;
	bool stopped() const;
	bool terminated() const;
	edb::pid_t process() const;
	edb::tid_t thread() const;
	int code() const;

private:
	IDebugEvent *impl_;
};

#endif
