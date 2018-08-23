/*
Copyright (C) 2006 - 2015 Evan Teran
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

#ifndef TYPES_20071127_H_
#define TYPES_20071127_H_

#include "API.h"
#include "Value.h"
#include "Address.h"

#include <sstream>
#include <type_traits>
#include <QString>

namespace edb {

enum EVENT_STATUS {
	DEBUG_STOP,                  // do nothing, the UI will instigate the next event
	DEBUG_CONTINUE,              // the event has been addressed, continue as normal
	DEBUG_CONTINUE_STEP,         // the event has been addressed, step as normal
	DEBUG_CONTINUE_BP,           // the event was a BP, which we need to ignore
	DEBUG_EXCEPTION_NOT_HANDLED, // pass the event unmodified back thread and continue
	DEBUG_NEXT_HANDLER
};

}

template<class T> typename std::enable_if<std::is_same<T,edb::value8 >::value ||
										  std::is_same<T,edb::value16>::value ||
										  std::is_same<T,edb::value32>::value ||
										  std::is_same<T,edb::value64>::value ||
										  std::is_same<T,edb::reg_t>::value   ||
										  std::is_same<T,edb::address_t>::value,
std::istream&>::type operator>>(std::istream& os, T& val) {
	os >> val.asUint();
	return os;
}

template<class T> typename std::enable_if<std::is_same<T,edb::value8 >::value ||
										  std::is_same<T,edb::value16>::value ||
										  std::is_same<T,edb::value32>::value ||
										  std::is_same<T,edb::value64>::value ||
										  std::is_same<T,edb::reg_t>::value   ||
										  std::is_same<T,edb::address_t>::value,
std::ostream&>::type operator<<(std::ostream& os, T val) {
	os << val.toUint();
	return os;
}

/* Comment Type */
struct Comment {
	edb::address_t 	address;
	QString        	comment;
};

#include "ArchTypes.h"
#endif
