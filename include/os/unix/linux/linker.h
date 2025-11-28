/*
 * Copyright (C) 2015 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef LINKER_H_20170103_
#define LINKER_H_20170103_

namespace edb::linux_struct {

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

#endif
