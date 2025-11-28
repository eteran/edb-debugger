/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef ARCH_TYPES_H_20071127_
#define ARCH_TYPES_H_20071127_

#include "Instruction.h"
#include "Types.h"

namespace edb {

using seg_reg_t   = value16;
using Instruction = CapstoneEDB::Instruction;
using Operand     = CapstoneEDB::Operand;

}

#endif
