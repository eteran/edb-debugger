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

#ifndef INSTRUCTION_20150908_H
#define INSTRUCTION_20150908_H

#include <string>
#include <memory>
#include <vector>
#include <cstdint>

namespace CapstoneEDB {

bool init(bool amd64);
bool isX86_64();

namespace Capstone {
#include <capstone/capstone.h>
}

class Instruction;
class Formatter;

class Operand
{
public:
	using Register=Capstone::x86_reg;
	struct Segment { // not enum class because we want to use this as array index etc.
		enum Reg : std::size_t
		{
			ES,
			CS,
			SS,
			DS,
			FS,
			GS,

			REG_INVALID=std::size_t(-1)
		};
	};
	enum Type {
		TYPE_INVALID       = 0x00000000,
		TYPE_REGISTER      = 0x00000100,
		TYPE_IMMEDIATE     = 0x00000200,
		TYPE_IMMEDIATE8    = 0x00000201,
		TYPE_IMMEDIATE16   = 0x00000202,
		TYPE_IMMEDIATE32   = 0x00000203,
		TYPE_IMMEDIATE64   = 0x00000204,
		TYPE_REL           = 0x00000300,
		// XXX: To determine relative operand size with capstone, we'd have to implement
		//      additional mini-disassembler on top of it. No native way is provided.
		//TYPE_REL8          = 0x00000301,
		//TYPE_REL16         = 0x00000302,
		//TYPE_REL32         = 0x00000303,
		TYPE_EXPRESSION    = 0x00000400,
		TYPE_EXPRESSION8   = 0x00000401,
		TYPE_EXPRESSION16  = 0x00000402,
		TYPE_EXPRESSION32  = 0x00000403,
		TYPE_EXPRESSION48  = 0x00000404,
		TYPE_EXPRESSION64  = 0x00000405,
		TYPE_EXPRESSION80  = 0x00000406,
		TYPE_EXPRESSION128 = 0x00000407,
		TYPE_EXPRESSION256 = 0x00000408,
		TYPE_EXPRESSION512 = 0x00000409,
		TYPE_ABSOLUTE      = 0x00000500,
		TYPE_MASK          = 0xffffff00
	};
	enum DisplacementType {
		DISP_NONE,
		DISP_PRESENT // XXX: Just to avoid changing API too early. DispType should actually be a bool.
	};
	struct absolute_t {
		uint16_t seg=0;
		uint32_t offset=0;
	};
	struct expression_t {

		Segment::Reg     segment=Segment::REG_INVALID;
		DisplacementType displacement_type=DISP_NONE;
		Register         base=Register::X86_REG_INVALID;
		Register         index=Register::X86_REG_INVALID;
		int32_t          displacement=0;
		uint8_t          scale=0;
	};
	Type general_type() const { return static_cast<Type>(complete_type() & ~0xff); }
	Type complete_type() const { return type_; }
	uint64_t relative_target() const { return imm_; };
	int32_t displacement() const { return expr_.displacement; }
	int64_t immediate() const { return imm_; } // FIXME: do we really want it signed?
	bool valid() const { return type_!=TYPE_INVALID; }
	operator void*() const { return reinterpret_cast<void*>(valid()); }
	const absolute_t absolute() const { return abs_; }
	const expression_t expression() const { return expr_; }
	Instruction* owner() const { return owner_; }
	Register reg() const { return reg_; }
	Operand(Instruction* instr, std::size_t numberInInstruction) : owner_(instr), numberInInstruction_(numberInInstruction) {}
	Operand(){}
private:
	union {
		Register     reg_;
		expression_t expr_;
		absolute_t   abs_;
		int64_t imm_;
	};
	Instruction* owner_=nullptr;
	Type         type_=TYPE_INVALID;
	std::size_t numberInInstruction_;

	friend class Instruction;
	friend class Formatter;
};

class Formatter;

class Instruction
{
public:
	using Operation=Capstone::x86_insn;
	enum Prefix {
		PREFIX_NONE             = 0x00000000,
		PREFIX_LOCK             = 0x00000001,
		PREFIX_REPNE            = 0x00000002,
		PREFIX_REP              = 0x00000004,

		PREFIX_CS               = 0x00000100,
		PREFIX_SS               = 0x00000200,
		PREFIX_DS               = 0x00000400,
		PREFIX_ES               = 0x00000800,
		PREFIX_FS               = 0x00001000,
		PREFIX_GS               = 0x00002000,
		PREFIX_BRANCH_NOT_TAKEN = 0x00004000,
		PREFIX_BRANCH_TAKEN     = 0x00008000,

		PREFIX_OPERAND          = 0x00010000,
		PREFIX_ADDRESS          = 0x01000000
	};
	typedef Operand operand_type;
public:
	static constexpr std::size_t MAX_SIZE=15;
	static constexpr std::size_t MAX_OPERANDS = 3;

public:
	Instruction(const void* first, const void* end, uint64_t rva) throw();
	Instruction(const Instruction&);
	Instruction& operator=(const Instruction&);
	bool valid() const { return valid_; }
	operator void*() const { return reinterpret_cast<void*>(valid()); }
	const uint8_t* bytes() const;
	std::size_t operand_count() const;
	const operand_type* operands() const { return operands_.data(); }
	void swap(Instruction &other);
	std::size_t size() const;
	uint64_t rva() const { return rva_; }
	std::string mnemonic() const;
	uint32_t prefix() const { return prefix_; }
	// Capstone doesn't provide any easy way to get total prefix length,
	// so this is currently unimplemented
	//std::size_t prefix_size() const;
	enum ConditionCode : uint8_t {
		CC_UNCONDITIONAL=0x10, // value must be higher than 0xF
		CC_CXZ,
		CC_ECXZ,
		CC_RCXZ,

		CC_B  = 2,   CC_C  = CC_B,
		CC_E  = 4,   CC_Z  = CC_E,
		CC_NA = 6,   CC_BE = CC_NA,
		CC_S  = 8,
		CC_P  = 0xA, CC_PE = CC_P,
		CC_L  = 0xC, CC_NGE= CC_L,
		CC_LE = 0xE, CC_NG = CC_LE,

		CC_NB = CC_B |1, CC_AE  = CC_NB,
		CC_NE = CC_E |1, CC_NZ  = CC_NE,
		CC_A  = CC_NA|1, CC_NBE = CC_A,
		CC_NS = CC_S |1,
		CC_NP = CC_P |1, CC_PO  = CC_NP,
		CC_NL = CC_L |1, CC_GE  = CC_NL,
		CC_NLE= CC_LE|1, CC_G   = CC_NLE
	};
	ConditionCode condition_code() const;
	Operation operation() const;

	// Check for any type of interrupt, including int3 etc.
	bool is_interrupt() const;
	// Check that instruction is int N (x86 0xCD N)
	bool is_int() const;
	// Check that instruction is call .*
	bool is_call() const;
	// Check that instruction is P5 sysenter
	bool is_sysenter() const;
	// Check that instruction is x86-64 syscall
	bool is_syscall() const;
	// Check that instruction is any type of return
	bool is_return() const;
	// Check that instruction is retn (x86 0xC3)
	bool is_ret() const;
	// Check that instruction is any type of jump
	bool is_jump() const;
	bool is_nop() const;
	bool is_halt() const;
	bool is_conditional_jump() const;
	bool is_unconditional_jump() const;
	bool is_conditional_fpu_move() const;
	bool is_conditional_gpr_move() const;
	bool is_conditional_set() const;
	bool is_conditional_move() const { return is_conditional_fpu_move() || is_conditional_gpr_move(); }
	bool is_terminator() const;
	bool is_fpu() const;
	// Check that instruction is an FPU instruction, taking only floating-point operands
	bool is_fpu_taking_float() const;
	// Check that instruction is an FPU instruction, one of operands of which is an integer
	bool is_fpu_taking_integer() const;
	// Check that instruction is an FPU instruction, one of operands of which is a packed BCD
	bool is_fpu_taking_bcd() const;

private:
	Capstone::cs_insn insn_;
	Capstone::cs_detail detail_;
	bool valid_=false;
	uint64_t rva_=0;
	uint8_t firstByte_=0;
	uint32_t prefix_=0;
	std::vector<Operand> operands_;

	void fillPrefix();

	friend class Formatter;
};

class Formatter
{
public:
	enum Syntax {
		SyntaxIntel,
		SyntaxATT
	};

	enum Capitalization {
		UpperCase,
		LowerCase
	};

	enum SmallNumberFormat {
		SmallNumAsHex,
		SmallNumAsDec
	};

	struct FormatOptions {
		Syntax            syntax;
		Capitalization    capitalization;
		SmallNumberFormat smallNumFormat;
	};
public:
	std::string to_string(const Instruction&) const;
	std::string to_string(const Operand&) const;
	std::string register_name(const Operand::Register) const;

	FormatOptions options() const { return options_; }
	void setOptions(const FormatOptions& options);
private:
	FormatOptions options_={SyntaxIntel,LowerCase,SmallNumAsDec};

	void checkCapitalize(std::string& str,bool canContainHex=true) const;
};

// TODO: move into Instruction class and remove from global scope
// NOTE(eteran): moved into the namespace, it'll be found by ADL, so we're all good :-)
//               so it may not be necessary to remove these after all. Sometimes free
//               functions can allow more flexibility in the API.
inline bool is_call(const CapstoneEDB::Instruction& insn) { return insn.is_call(); }
inline bool is_jump(const CapstoneEDB::Instruction& insn) { return insn.is_jump(); }
inline bool is_ret(const CapstoneEDB::Instruction& insn) { return insn.is_ret(); }
inline bool is_terminator(const CapstoneEDB::Instruction& insn) { return insn.is_terminator(); }
inline bool is_conditional_jump(const CapstoneEDB::Instruction& insn) { return insn.is_conditional_jump(); }
inline bool is_unconditional_jump(const CapstoneEDB::Instruction& insn) { return insn.is_unconditional_jump(); }

}



#endif
