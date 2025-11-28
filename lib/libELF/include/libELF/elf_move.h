/*
 * Copyright (C) 2012 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * Copyright (C) 1995-2003,2004,2005,2006,2007,2008,2009,2010,2011
 * Free Software Foundation, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef ELF_MOVE_H_20121007_
#define ELF_MOVE_H_20121007_

#include "elf_types.h"

// Move records.
struct elf32_move {
	elf32_xword m_value;  // Symbol value.
	elf32_word m_info;    // Size and index.
	elf32_word m_poffset; // Symbol offset.
	elf32_half m_repeat;  // Repeat count.
	elf32_half m_stride;  // Stride info.
};

struct elf64_move {
	elf64_xword m_value;   // Symbol value.
	elf64_xword m_info;    // Size and index.
	elf64_xword m_poffset; // Symbol offset.
	elf64_half m_repeat;   // Repeat count.
	elf64_half m_stride;   // Stride info.
};

// Macro to construct move records.
template <class T>
constexpr T ELF32_M_SYM(T info) {
	return info >> 8;
}

template <class T>
constexpr auto ELF32_M_SIZE(T info) {
	return static_cast<uint8_t>(info);
}

template <class T1, class T2>
constexpr auto ELF32_M_INFO(T1 sym, T2 size) {
	return (sym << 8) + static_cast<uint8_t>(size);
}

template <class T>
constexpr T ELF64_M_SYM(T info) {
	return ELF32_M_SYM(info);
}

template <class T>
constexpr auto ELF64_M_SIZE(T info) {
	return ELF32_M_SIZE(info);
}

template <class T1, class T2>
constexpr auto ELF64_M_INFO(T1 sym, T2 size) {
	return ELF32_M_INFO(sym, size);
}

#endif
