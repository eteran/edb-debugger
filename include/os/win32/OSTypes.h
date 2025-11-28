/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef OSTYPES_H_20070116_
#define OSTYPES_H_20070116_

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

namespace edb {
using pid_t = DWORD;
using uid_t = DWORD; // TODO(eteran): I think this needs to be an SID to make any sense
using tid_t = DWORD;
}

#endif
