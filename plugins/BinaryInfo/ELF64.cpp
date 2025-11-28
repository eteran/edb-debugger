/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ELFXX.h"
#include "IDebugger.h"
#include "edb.h"
#include "string_hash.h"

namespace BinaryInfoPlugin {

/**
 * @brief ELF64::native
 * @return true if this binary is native to the arch edb was built for
 */
template <>
bool ELF64::native() const {
#if defined(EDB_X86) || defined(EDB_X86_64)
	return edb::v1::debugger_core->cpuType() == edb::string_hash("x86-64");
#elif defined(EDB_ARM32) || defined(EDB_ARM64)
	return edb::v1::debugger_core->cpuType() == edb::string_hash("AArch64");
#else
#error "Unsupported Architecture"
#endif
}

}
