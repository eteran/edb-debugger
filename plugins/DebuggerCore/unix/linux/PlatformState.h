/*
Copyright (C) 2006 - 2015 Evan Teran
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

#ifndef PLATFORMSTATE_20110330_H_
#define PLATFORMSTATE_20110330_H_

#include "IState.h"
#include "Types.h"
#include <cstddef>
#include <sys/user.h>
#include "edb.h"

namespace DebuggerCore {

using std::size_t;

static constexpr size_t IA32_GPR_COUNT=8;
static constexpr size_t IA32_GPR_LOW_ADDRESSABLE_COUNT=4; // al,cl,dl,bl
static constexpr size_t AMD64_GPR_COUNT=16;
static constexpr size_t AMD64_GPR_LOW_ADDRESSABLE_COUNT=16; // all GPRs' low bytes are addressable in 64 bit mode
static constexpr size_t IA32_XMM_REG_COUNT=IA32_GPR_COUNT;
static constexpr size_t AMD64_XMM_REG_COUNT=AMD64_GPR_COUNT;
static constexpr size_t IA32_YMM_REG_COUNT=IA32_GPR_COUNT;
static constexpr size_t AMD64_YMM_REG_COUNT=AMD64_GPR_COUNT;
static constexpr size_t IA32_ZMM_REG_COUNT=IA32_GPR_COUNT;
static constexpr size_t AMD64_ZMM_REG_COUNT=32;
static constexpr size_t MAX_GPR_COUNT=AMD64_GPR_COUNT;
static constexpr size_t MAX_GPR_LOW_ADDRESSABLE_COUNT=AMD64_GPR_LOW_ADDRESSABLE_COUNT;
static constexpr size_t MAX_GPR_HIGH_ADDRESSABLE_COUNT=4; // ah,ch,dh,bh
static constexpr size_t MAX_DBG_REG_COUNT=8;
static constexpr size_t MAX_SEG_REG_COUNT=6;
static constexpr size_t MAX_FPU_REG_COUNT=8;
static constexpr size_t MAX_MMX_REG_COUNT=MAX_FPU_REG_COUNT;
static constexpr size_t MAX_XMM_REG_COUNT=AMD64_XMM_REG_COUNT;
static constexpr size_t MAX_YMM_REG_COUNT=AMD64_YMM_REG_COUNT;
static constexpr size_t MAX_ZMM_REG_COUNT=AMD64_ZMM_REG_COUNT;

#ifdef EDB_X86
typedef struct user_regs_struct UserRegsStructX86;
typedef struct user_fpregs_struct UserFPRegsStructX86;
typedef struct user_fpxregs_struct UserFPXRegsStructX86;
// Dummies to avoid missing compile-time checks for conversion code.
// Actual layout is irrelevant since the code is not going to be executed
struct UserFPRegsStructX86_64 {
	uint16_t cwd;
	uint16_t swd;
	uint16_t ftw;
	uint16_t fop; // last instruction opcode
	uint64_t rip; // last instruction EIP
	uint64_t rdp; // last operand pointer
	uint32_t mxcsr;
	uint32_t mxcr_mask;
	uint32_t st_space[32];
	uint32_t xmm_space[64];
	uint32_t padding[24];
};
struct UserRegsStructX86_64 {
	uint64_t r15;
	uint64_t r14;
	uint64_t r13;
	uint64_t r12;
	uint64_t rbp;
	uint64_t rbx;
	uint64_t r11;
	uint64_t r10;
	uint64_t r9;
	uint64_t r8;
	uint64_t rax;
	uint64_t rcx;
	uint64_t rdx;
	uint64_t rsi;
	uint64_t rdi;
	uint64_t orig_rax;
	uint64_t rip;
	uint64_t cs;
	uint64_t eflags;
	uint64_t rsp;
	uint64_t ss;
	uint64_t fs_base;
	uint64_t gs_base;
	uint64_t ds;
	uint64_t es;
	uint64_t fs;
	uint64_t gs;
};
#elif defined EDB_X86_64
typedef user_regs_struct UserRegsStructX86_64;
typedef user_fpregs_struct UserFPRegsStructX86_64;
// Dummies to avoid missing compile-time checks for conversion code
// Actual layout is irrelevant since the code is not going to be executed
struct UserRegsStructX86 {
	uint32_t ebx;
	uint32_t ecx;
	uint32_t edx;
	uint32_t esi;
	uint32_t edi;
	uint32_t ebp;
	uint32_t eax;
	uint32_t xds;
	uint32_t xes;
	uint32_t xfs;
	uint32_t xgs;
	uint32_t orig_eax;
	uint32_t eip;
	uint32_t xcs;
	uint32_t eflags;
	uint32_t esp;
	uint32_t xss;
};
struct UserFPXRegsStructX86 {
	uint16_t cwd;
	uint16_t swd;
	uint16_t twd;
	uint16_t fop; // last instruction opcode
	uint32_t fip; // last instruction EIP
	uint32_t fcs; // last instruction CS
	uint32_t foo; // last operand offset
	uint32_t fos; // last operand selector
	uint32_t mxcsr;
	uint32_t reserved;
	uint32_t st_space[32];   /* 8*16 bytes for each FP-reg = 128 bytes */
	uint32_t xmm_space[32];  /* 8*16 bytes for each XMM-reg = 128 bytes */
	uint32_t padding[56];
};
struct UserFPRegsStructX86 {
	uint32_t cwd;
	uint32_t swd;
	uint32_t twd;
	uint32_t fip; // last instruction EIP
	uint32_t fcs; // last instruction CS
	uint32_t foo; // last operand offset
	uint32_t fos; // last operand selector
	uint32_t st_space [20];
};
#endif

struct PrStatus_X86
{
	uint32_t ebx;
	uint32_t ecx;
	uint32_t edx;
	uint32_t esi;
	uint32_t edi;
	uint32_t ebp;
	uint32_t eax;
	uint32_t ds;
	uint32_t es;
	uint32_t fs;
	uint32_t gs;
	uint32_t orig_eax;
	uint32_t eip;
	uint32_t cs;
	uint32_t eflags;
	uint32_t esp;
	uint32_t ss;
};
static_assert(sizeof(PrStatus_X86)==68,"PrStatus_X86 is messed up!");

struct PrStatus_X86_64
{
	uint64_t r15;
	uint64_t r14;
	uint64_t r13;
	uint64_t r12;
	uint64_t rbp;
	uint64_t rbx;
	uint64_t r11;
	uint64_t r10;
	uint64_t r9;
	uint64_t r8;
	uint64_t rax;
	uint64_t rcx;
	uint64_t rdx;
	uint64_t rsi;
	uint64_t rdi;
	uint64_t orig_rax;
	uint64_t rip;
	uint64_t cs;
	uint64_t rflags;
	uint64_t rsp;
	uint64_t ss;
	uint64_t fs_base;
	uint64_t gs_base;
	uint64_t ds;
	uint64_t es;
	uint64_t fs;
	uint64_t gs;
};
static_assert(sizeof(PrStatus_X86_64)==216,"PrStatus_X86_64 is messed up!");


// Masks for XCR0 feature enabled bits
#define X86_XSTATE_X87_MASK  X87_XSTATE_X87
#define X86_XSTATE_SSE_MASK  (X87_XSTATE_X87|X87_XSTATE_SSE)
struct X86XState
{
	uint16_t cwd;
	uint16_t swd;
	uint16_t twd;
	uint16_t fop; // last instruction opcode
	uint32_t fioff; // last instruction EIP
	uint32_t fiseg; // last instruction CS in 32 bit mode, high 32 bits of RIP in 64 bit mode
	uint32_t fooff; // last operand offset
	uint32_t foseg; // last operand selector in 32 bit mode, high 32 bits of FDP in 64 bit mode
	uint32_t mxcsr;
	uint32_t mxcsr_mask; // FIXME
	uint8_t st_space[16*8]; // 8 16-byte fields
	uint8_t xmm_space[16*16]; // 16 16-byte fields, regardless of XMM_REG_COUNT
	uint8_t padding[48];
	union {
		uint64_t xcr0;
		uint8_t sw_usable_bytes[48];
	};
	union {
		uint64_t xstate_bv;
		uint8_t xstate_hdr_bytes[64];
	};
	uint8_t ymmh_space[16*16];

	// The extended state feature bits
	enum FeatureBit : uint64_t {
		FEATURE_X87     = 1<<0,
		FEATURE_SSE     = 1<<1,
		FEATURE_AVX     = 1<<2,
		// MPX adds two feature bits
		FEATURE_BNDREGS = 1<<3,
		FEATURE_BNDCFG  = 1<<4,
		FEATURE_MPX     = FEATURE_BNDREGS|FEATURE_BNDCFG,
		// AVX-512 adds three feature bits
		FEATURE_K       = 1<<5,
		FEATURE_ZMM_H   = 1<<6,
		FEATURE_ZMM     = 1<<7,
		FEATURE_AVX512  = FEATURE_K|FEATURE_ZMM_H|FEATURE_ZMM,
	};
	// Possible sizes of X86_XSTATE
	static constexpr std::size_t SSE_SIZE=576;
	static constexpr std::size_t AVX_SIZE=832;
	static constexpr std::size_t BNDREGS_SIZE=1024;
	static constexpr std::size_t BNDCFG_SIZE=1088;
	static constexpr std::size_t AVX512_SIZE=2688;
	static constexpr std::size_t MAX_SIZE=2688;
};
static_assert(std::is_standard_layout<X86XState>::value,"X86XState struct is supposed to have standard layout");
static_assert(offsetof(X86XState,st_space)==32,"ST space should appear at offset 32");
static_assert(offsetof(X86XState,xmm_space)==160,"XMM space should appear at offset 160");
static_assert(offsetof(X86XState,xcr0)==464,"XCR0 should appear at offset 464");
static_assert(offsetof(X86XState,ymmh_space)==576,"YMM_H space should appear at offset 576");

class PlatformState : public IState {
	friend class DebuggerCore;
	friend class PlatformThread;

public:
	PlatformState();

public:
	virtual IState *clone() const;

public:
	virtual QString flags_to_string() const;
	virtual QString flags_to_string(edb::reg_t flags) const;
	virtual Register value(const QString &reg) const;
	virtual Register instruction_pointer_register() const;
	virtual Register flags_register() const;
	virtual edb::address_t frame_pointer() const;
	virtual edb::address_t instruction_pointer() const;
	virtual edb::address_t stack_pointer() const;
	virtual edb::reg_t debug_register(size_t n) const;
	virtual edb::reg_t flags() const;
	virtual int fpu_stack_pointer() const;
	virtual edb::value80 fpu_register(size_t n) const;
	virtual bool fpu_register_is_empty(std::size_t n) const;
	virtual QString fpu_register_tag_string(std::size_t n) const;
	virtual edb::value16 fpu_control_word() const;
	virtual edb::value16 fpu_status_word() const;
	virtual edb::value16 fpu_tag_word() const;
	virtual void adjust_stack(int bytes);
	virtual void clear();
	virtual bool empty() const;
	virtual void set_debug_register(size_t n, edb::reg_t value);
	virtual void set_flags(edb::reg_t flags);
	virtual void set_instruction_pointer(edb::address_t value);
	virtual void set_register(const Register& reg);
	virtual void set_register(const QString &name, edb::reg_t value);
	virtual Register mmx_register(size_t n) const;
	virtual Register xmm_register(size_t n) const;
	virtual Register ymm_register(size_t n) const;
	virtual Register gp_register(size_t n) const;
	bool is64Bit() const { return edb::v1::debuggeeIs64Bit(); }
	bool is32Bit() const { return edb::v1::debuggeeIs32Bit(); }
	size_t dbg_reg_count() const { return MAX_DBG_REG_COUNT; }
	size_t seg_reg_count() const { return MAX_SEG_REG_COUNT; }
	size_t fpu_reg_count() const { return MAX_FPU_REG_COUNT; }
	size_t mmx_reg_count() const { return MAX_MMX_REG_COUNT; }
	size_t xmm_reg_count() const { return is64Bit() ? AMD64_XMM_REG_COUNT : IA32_XMM_REG_COUNT; }
	size_t ymm_reg_count() const { return is64Bit() ? AMD64_YMM_REG_COUNT : IA32_YMM_REG_COUNT; }
	size_t zmm_reg_count() const { return is64Bit() ? AMD64_ZMM_REG_COUNT : IA32_ZMM_REG_COUNT; }
	size_t gpr64_count() const { return is64Bit() ? AMD64_GPR_COUNT : 0; }
	size_t gpr_count() const { return is64Bit() ? AMD64_GPR_COUNT : IA32_GPR_COUNT; }
	size_t gpr_low_addressable_count() const { return is64Bit() ? AMD64_GPR_LOW_ADDRESSABLE_COUNT : IA32_GPR_LOW_ADDRESSABLE_COUNT; }
	size_t gpr_high_addressable_count() const { return MAX_GPR_HIGH_ADDRESSABLE_COUNT; }

	const char* IPName() const { return is64Bit() ? x86.IP64Name : x86.IP32Name; }
	const char* flagsName() const { return is64Bit() ? x86.flags64Name : x86.flags32Name; }
	const std::array<const char*,MAX_GPR_COUNT>& GPRegNames() const { return is64Bit() ? x86.GPReg64Names : x86.GPReg32Names; }
private:
	// The whole AVX* state. XMM and YMM registers are lower parts of ZMM ones.
	struct AVX {
		std::array<edb::value512,MAX_ZMM_REG_COUNT> zmmStorage;

		edb::value32 mxcsr;
		edb::value32 mxcsrMask;
		edb::value64 xcr0;
		bool xmmFilledIA32=false;
		bool xmmFilledAMD64=false; // This can be false when filled from e.g. FPXregs
		bool ymmFilled=false;
		bool zmmFilled=false;
		bool mxcsrMaskFilled=false;

		void clear();
		bool empty() const;
		edb::value128 xmm(size_t index) const;
		void setXMM(size_t index,edb::value128);
		edb::value256 ymm(size_t index) const;
		void setYMM(size_t index,edb::value256);
		void setYMM(size_t index,edb::value128 low, edb::value128 high);
		edb::value512 zmm(size_t index) const;
		void setZMM(size_t index,edb::value512);

		static constexpr const char* mxcsrName="mxcsr";
	} avx;

	// x87 state
	struct X87 {
		std::array<edb::value80,MAX_FPU_REG_COUNT> R; // Rx registers
		edb::address_t instPtrOffset;
		edb::address_t dataPtrOffset;
		edb::value16 instPtrSelector;
		edb::value16 dataPtrSelector;
		edb::value16 controlWord;
		edb::value16 statusWord;
		edb::value16 tagWord;
		edb::value16 opCode;
		bool filled=false;
		bool opCodeFilled=false;

		void clear();
		bool empty() const;
		std::size_t stackPointer() const;
		// Convert from ST(n) index n to Rx index x
		std::size_t RIndexToSTIndex(std::size_t index) const;
		std::size_t STIndexToRIndex(std::size_t index) const;
		// Restore the full FPU Tag Word from the ptrace-filtered version
		edb::value16 restoreTagWord(uint16_t twd) const;
		std::uint16_t reducedTagWord() const;
		int tag(std::size_t n) const;
		edb::value80 st(std::size_t n) const;
		edb::value80& st(std::size_t n);
		enum Tag {
			TAG_VALID=0,
			TAG_ZERO=1,
			TAG_SPECIAL=2,
			TAG_EMPTY=3
		};
	private:
		int recreateTag(const edb::value80 value) const;
		int makeTag(std::size_t n, uint16_t twd) const;
	} x87;

	// i386-inherited (and expanded on x86_64) state
	struct X86 {
		std::array<edb::reg_t,MAX_GPR_COUNT> GPRegs;
		std::array<edb::reg_t,MAX_DBG_REG_COUNT> dbgRegs;
		edb::reg_t orig_ax;
		edb::reg_t flags; // whole flags register: EFLAGS/RFLAGS
		edb::address_t IP; // program counter: EIP/RIP
		std::array<edb::seg_reg_t,MAX_SEG_REG_COUNT> segRegs;
		std::array<edb::address_t,MAX_SEG_REG_COUNT> segRegBases;
		std::array<bool,MAX_SEG_REG_COUNT> segRegBasesFilled={{false}};
		bool gpr64Filled=false;
		bool gpr32Filled=false;

		void clear();
		bool empty() const;

		enum GPRIndex : std::size_t {
			EAX,RAX=EAX,
			ECX,RCX=ECX,
			EDX,RDX=EDX,
			EBX,RBX=EBX,
			ESP,RSP=ESP,
			EBP,RBP=EBP,
			ESI,RSI=ESI,
			EDI,RDI=EDI,
			R8,
			R9,
			R10,
			R11,
			R12,
			R13,
			R14,
			R15
		};
		enum SegRegIndex : std::size_t {
			ES,
			CS,
			SS,
			DS,
			FS,
			GS
		};
		static constexpr const char* origEAXName="orig_eax";
		static constexpr const char* origRAXName="orig_rax";
		static constexpr const char* IP64Name="rip";
		static constexpr const char* IP32Name="eip";
		static constexpr const char* IP16Name="ip";
		static constexpr const char* flags64Name="rflags";
		static constexpr const char* flags32Name="eflags";
		static constexpr const char* flags16Name="flags";
		// gcc 4.8 fails to understand inline initialization of std::array, so define these the old way
		static const std::array<const char*,MAX_GPR_COUNT> GPReg64Names;
		static const std::array<const char*,MAX_GPR_COUNT> GPReg32Names;
		static const std::array<const char*,MAX_GPR_COUNT> GPReg16Names;
		static const std::array<const char*,MAX_GPR_LOW_ADDRESSABLE_COUNT> GPReg8LNames;
		static const std::array<const char*,MAX_GPR_HIGH_ADDRESSABLE_COUNT> GPReg8HNames;
		static const std::array<const char*,MAX_SEG_REG_COUNT> segRegNames;
	} x86;

	bool dbgIndexValid(size_t n) const { return n<dbg_reg_count(); }
	bool gprIndexValid(size_t n) const { return n<gpr_count(); }
	bool fpuIndexValid(size_t n) const { return n<fpu_reg_count(); }
	bool mmxIndexValid(size_t n) const { return n<mmx_reg_count(); }
	bool xmmIndexValid(size_t n) const { return n<xmm_reg_count(); }
	bool ymmIndexValid(size_t n) const { return n<ymm_reg_count(); }
	bool zmmIndexValid(size_t n) const { return n<zmm_reg_count(); }


	void fillFrom(const UserRegsStructX86& regs);
	void fillFrom(const UserRegsStructX86_64& regs);
	void fillFrom(const PrStatus_X86& regs);
	void fillFrom(const PrStatus_X86_64& regs);
	void fillFrom(const UserFPRegsStructX86& regs);
	void fillFrom(const UserFPRegsStructX86_64& regs);
	void fillFrom(const UserFPXRegsStructX86& regs);
	void fillFrom(const X86XState& regs, std::size_t sizeFromKernel);

	void fillStruct(UserRegsStructX86& regs) const;
	void fillStruct(UserRegsStructX86_64& regs) const;
	void fillStruct(PrStatus_X86_64& regs) const;
	void fillStruct(UserFPRegsStructX86& regs) const;
	void fillStruct(UserFPRegsStructX86_64& regs) const;
	void fillStruct(UserFPXRegsStructX86& regs) const;
	size_t fillStruct(X86XState& regs) const;
};

}

#endif

