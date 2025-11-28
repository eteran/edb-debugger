/*
 * Copyright (C) 2015 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef LIB_HARDWARE_BREAKPOINTS_H_20191119_
#define LIB_HARDWARE_BREAKPOINTS_H_20191119_

#include "edb.h"

namespace HardwareBreakpointsPlugin {

constexpr int Register1     = 0;
constexpr int Register2     = 1;
constexpr int Register3     = 2;
constexpr int Register4     = 3;
constexpr int RegisterCount = 4;

struct BreakpointState {
	bool enabled;
	edb::address_t addr;
	int type;
	int size;
};

enum BreakpointStatus {
	Valid,
	AlignmentError,
	SizeError
};

BreakpointState breakpoint_state(const State *state, int num);
void set_breakpoint_state(State *state, int num, const BreakpointState &bp_state);
BreakpointStatus validate_breakpoint(const BreakpointState &bp_state);

}

#endif
