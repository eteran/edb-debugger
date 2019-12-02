/*
Copyright (C) 2015 - 2015 Evan Teran
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

#ifndef ITHREAD_H_20150529_
#define ITHREAD_H_20150529_

#include "OSTypes.h"
#include "Status.h"
#include "Types.h"

class State;

class IThread {
public:
	virtual ~IThread() = default;

public:
	virtual edb::tid_t tid() const                    = 0;
	virtual QString name() const                      = 0;
	virtual int priority() const                      = 0;
	virtual edb::address_t instructionPointer() const = 0;
	virtual QString runState() const                  = 0;

public:
	virtual void getState(State *state)       = 0;
	virtual void setState(const State &state) = 0;

public:
	virtual Status step()                          = 0;
	virtual Status step(edb::EventStatus status)   = 0;
	virtual Status resume()                        = 0;
	virtual Status resume(edb::EventStatus status) = 0;

public:
	virtual bool isPaused() const = 0;
};

#endif
