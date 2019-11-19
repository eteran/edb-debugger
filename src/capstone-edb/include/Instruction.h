/*
Copyright (C) 2015 Ruslan Kabatsayev <b7.10110111@gmail.com>

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

#ifndef INSTRUCTION_H_20150908_
#define INSTRUCTION_H_20150908_

#include "API.h"
#include "Formatter.h"
#include "Operand.h"
#include <capstone/capstone.h>
#include <cstdint>
#include <string>

class QString;

namespace CapstoneEDB {

enum class Architecture {
	ARCH_X86,
	ARCH_AMD64,
	ARCH_ARM32_ARM,
	ARCH_ARM32_THUMB,
	ARCH_ARM64
};

bool init(Architecture arch);

class Instruction;
class Formatter;

class EDB_EXPORT Instruction {
	friend class Formatter;
	friend class Operand;

public:
#if defined(EDB_X86) || defined(EDB_X86_64)
	static constexpr std::size_t MaxSize = 15;
#elif defined(EDB_ARM32) || defined(EDB_ARM64)
	static constexpr std::size_t MaxSize = 4;
#endif

public:
	Instruction(const void *first, const void *end, uint64_t rva) noexcept;
	Instruction(const Instruction &) = delete;
	Instruction &operator=(const Instruction &) = delete;
	Instruction(Instruction &&) noexcept;
	Instruction &operator=(Instruction &&) noexcept;
	~Instruction();

public:
	bool valid() const {
		return insn_;
	}

	explicit operator bool() const {
		return valid();
	}

public:
	int operation() const { return insn_ ? insn_->id : 0; }
	std::size_t operandCount() const {
#if defined(EDB_X86) || defined(EDB_X86_64)
		return insn_ ? insn_->detail->x86.op_count : 0;
#elif defined(EDB_ARM32) || defined(EDB_ARM64)
		return insn_ ? insn_->detail->arm.op_count : 0;
#else
#error "What to return here?"
#endif
	}
	std::size_t byteSize() const { return insn_ ? insn_->size : 1; }
	uint64_t rva() const { return insn_ ? insn_->address : rva_; }
	std::string mnemonic() const { return insn_ ? insn_->mnemonic : std::string(); }
	const uint8_t *bytes() const { return insn_ ? insn_->bytes : &byte0_; }

public:
	Operand operator[](size_t n) const;
	Operand operand(size_t n) const;

public:
	const cs_insn *native() const {
		return insn_;
	}

public:
	cs_insn *operator->() { return insn_; }
	const cs_insn *operator->() const { return insn_; }

public:
	void swap(Instruction &other);

public:
	enum ConditionCode : uint8_t {
#if defined(EDB_X86) || defined(EDB_X86_64)
		CC_UNCONDITIONAL = 0x10, // value must be higher than 0xF
		CC_CXZ,
		CC_ECXZ,
		CC_RCXZ,

		CC_B   = 2,
		CC_C   = CC_B,
		CC_E   = 4,
		CC_Z   = CC_E,
		CC_NA  = 6,
		CC_BE  = CC_NA,
		CC_S   = 8,
		CC_P   = 0xA,
		CC_PE  = CC_P,
		CC_L   = 0xC,
		CC_NGE = CC_L,
		CC_LE  = 0xE,
		CC_NG  = CC_LE,

		CC_NB  = CC_B | 1,
		CC_AE  = CC_NB,
		CC_NE  = CC_E | 1,
		CC_NZ  = CC_NE,
		CC_A   = CC_NA | 1,
		CC_NBE = CC_A,
		CC_NS  = CC_S | 1,
		CC_NP  = CC_P | 1,
		CC_PO  = CC_NP,
		CC_NL  = CC_L | 1,
		CC_GE  = CC_NL,
		CC_NLE = CC_LE | 1,
		CC_G   = CC_NLE
#elif defined(EDB_ARM32)
		CC_EQ = 0,
		CC_NE,
		CC_HS,
		CC_LO,
		CC_MI,
		CC_PL,
		CC_VS,
		CC_VC,
		CC_HI,
		CC_LS,
		CC_GE,
		CC_LT,
		CC_GT,
		CC_LE,
		CC_AL,
#else
#error "Not implemented"
#endif
	};

	ConditionCode conditionCode() const;

private:
	cs_insn *insn_ = nullptr;

	// we have our own copies of this data so we can give something meaningful
	// even during a failed disassembly
	uint8_t byte0_ = 0;
	uint64_t rva_  = 0;
};

}

#include "Inspection.h"

#endif
