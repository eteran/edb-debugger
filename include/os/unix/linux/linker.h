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

#ifndef LINKER_H_20170103_
#define LINKER_H_20170103_

namespace edb {
namespace linux_struct {

// Bitness-templated version of struct r_debug defined in link.h
template <class Addr>
struct r_debug {
	int r_version;
	Addr r_map; // struct link_map*
	Addr r_brk;
	enum {
		RT_CONSISTENT,
		RT_ADD,
		RT_DELETE
	} r_state;
	Addr r_ldbase;
};

// Bitness-templated version of struct link_map defined in link.h
template <class Addr>
struct link_map {
	Addr l_addr;
	Addr l_name;         // char*
	Addr l_ld;           // ElfW(Dyn)*
	Addr l_next, l_prev; // struct link_map*
};

}
}

#endif
