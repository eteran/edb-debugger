/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef OSTYPES_H_20060625_
#define OSTYPES_H_20060625_

#include <sys/types.h> // for pid_t/uid_t

namespace edb {
using ::pid_t;
using ::uid_t;
using tid_t = ::pid_t;
}

#endif
