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

#ifndef ITHREAD_20150529_H_
#define ITHREAD_20150529_H_

#include "Types.h"
#include <memory>

class ThreadInfo;

class IThread {
public:
	typedef std::shared_ptr<IThread> pointer;
	
public:
	virtual ~IThread() {}
	
public:
	virtual edb::tid_t tid() const = 0;
	
public:
	virtual void resume() = 0;
	virtual void step() = 0;
	
public:
	// TODO(eteran): maybe break this up into piece by piece functions
	virtual ThreadInfo info() const = 0;
};

#endif
