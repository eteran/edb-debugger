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

#include "PlatformState.h"
#include "Util.h"
#include "FloatX.h"
#include <unordered_map>
#include <QRegExp>
#include <QDebug>

namespace DebuggerCore {

constexpr const char* PlatformState::AVX::mxcsrName;
constexpr const char* PlatformState::X86::IP64Name;
constexpr const char* PlatformState::X86::IP32Name;
constexpr const char* PlatformState::X86::IP16Name;
constexpr const char* PlatformState::X86::origEAXName;
constexpr const char* PlatformState::X86::origRAXName;
constexpr const char* PlatformState::X86::flags64Name;
constexpr const char* PlatformState::X86::flags32Name;
constexpr const char* PlatformState::X86::flags16Name;
const std::array<const char*,MAX_GPR_COUNT> PlatformState::X86::GPReg64Names={
	"rax",
	"rcx",
	"rdx",
	"rbx",
	"rsp",
	"rbp",
	"rsi",
	"rdi",
	"r8",
	"r9",
	"r10",
	"r11",
	"r12",
	"r13",
	"r14",
	"r15"
};
const std::array<const char*,MAX_GPR_COUNT> PlatformState::X86::GPReg32Names={
	"eax",
	"ecx",
	"edx",
	"ebx",
	"esp",
	"ebp",
	"esi",
	"edi",
	"r8d",
	"r9d",
	"r10d",
	"r11d",
	"r12d",
	"r13d",
	"r14d",
	"r15d"
};

const std::array<const char*,MAX_GPR_COUNT> PlatformState::X86::GPReg16Names={
	"ax",
	"cx",
	"dx",
	"bx",
	"sp",
	"bp",
	"si",
	"di"
   ,"r8w",
	"r9w",
	"r10w",
	"r11w",
	"r12w",
	"r13w",
	"r14w",
	"r15w"
};
const std::array<const char*,MAX_GPR_LOW_ADDRESSABLE_COUNT> PlatformState::X86::GPReg8LNames={
	"al",
	"cl",
	"dl",
	"bl",
	"spl",
	"bpl",
	"sil",
	"dil"
   ,"r8b",
	"r9b",
	"r10b",
	"r11b",
	"r12b",
	"r13b",
	"r14b",
	"r15b"
};
const std::array<const char*,MAX_GPR_HIGH_ADDRESSABLE_COUNT> PlatformState::X86::GPReg8HNames={
	"ah",
	"ch",
	"dh",
	"bh"
};
const std::array<const char*,MAX_SEG_REG_COUNT> PlatformState::X86::segRegNames={
	"es",
	"cs",
	"ss",
	"ds",
	"fs",
	"gs"
};

void PlatformState::fillFrom(const UserRegsStructX86& regs) {
	// Don't touch higher parts to avoid zeroing out bad value mark
	std::memcpy(&x86.GPRegs[X86::EAX],&regs.eax,sizeof(regs.eax));
	std::memcpy(&x86.GPRegs[X86::ECX],&regs.ecx,sizeof(regs.ecx));
	std::memcpy(&x86.GPRegs[X86::EDX],&regs.edx,sizeof(regs.edx));
	std::memcpy(&x86.GPRegs[X86::EBX],&regs.ebx,sizeof(regs.ebx));
	std::memcpy(&x86.GPRegs[X86::ESP],&regs.esp,sizeof(regs.esp));
	std::memcpy(&x86.GPRegs[X86::EBP],&regs.ebp,sizeof(regs.ebp));
	std::memcpy(&x86.GPRegs[X86::ESI],&regs.esi,sizeof(regs.esi));
	std::memcpy(&x86.GPRegs[X86::EDI],&regs.edi,sizeof(regs.edi));
	std::memcpy(&x86.orig_ax,&regs.orig_eax,sizeof(regs.orig_eax));
	std::memcpy(&x86.flags,&regs.eflags,sizeof(regs.eflags));
	std::memcpy(&x86.IP,&regs.eip,sizeof(regs.eip));
	x86.segRegs[X86::ES] = regs.xes;
	x86.segRegs[X86::CS] = regs.xcs;
	x86.segRegs[X86::SS] = regs.xss;
	x86.segRegs[X86::DS] = regs.xds;
	x86.segRegs[X86::FS] = regs.xfs;
	x86.segRegs[X86::GS] = regs.xgs;
	x86.gpr32Filled=true;
}

std::size_t PlatformState::X87::stackPointer() const {
	return (statusWord&0x3800)>>11;
}

std::size_t PlatformState::X87::RIndexToSTIndex(std::size_t n) const {

	n=(n+8-stackPointer()) % 8;
	return n;
}

std::size_t PlatformState::X87::STIndexToRIndex(std::size_t n) const {

	n=(n+stackPointer()) % 8;
	return n;
}

int PlatformState::X87::recreateTag(edb::value80 value) const {
	switch(floatType(value))
	{
	case FloatValueClass::Zero:
		return TAG_ZERO;
	case FloatValueClass::Normal:
		return TAG_VALID;
	default:
		return TAG_SPECIAL;
	}
}

edb::value80 PlatformState::X87::st(std::size_t n) const {
	return R[STIndexToRIndex(n)];
}

edb::value80& PlatformState::X87::st(std::size_t n) {
	return R[STIndexToRIndex(n)];
}

int PlatformState::X87::makeTag(std::size_t n, uint16_t twd) const {
	int minitag=(twd>>n)&0x1;
	return minitag ? recreateTag(R[n]) : TAG_EMPTY;
}

int PlatformState::X87::tag(std::size_t n) const {
	return (tagWord>>(2*n)) & 0x3;
}

edb::value16 PlatformState::X87::restoreTagWord(uint16_t twd) const {
	uint16_t tagWord=0;
	for(std::size_t n=0;n<MAX_FPU_REG_COUNT;++n)
		tagWord |= makeTag(n,twd)<<(2*n);
	return edb::value16(tagWord);
}

std::uint16_t PlatformState::X87::reducedTagWord() const {
	// Algorithm is the same as in linux/arch/x86/kernel/i387.c: twd_i387_to_fxsr()

	// Transforming each pair of bits into 01 (valid) or 00 (empty)
	unsigned int result = ~tagWord;
	result = (result | (result>>1)) & 0x5555; // 0102030405060708
	// moving the valid bits to the lower byte
	result = (result | (result>>1)) & 0x3333; // 0012003400560078
	result = (result | (result>>2)) & 0x0f0f; // 0000123400005678
	result = (result | (result>>4)) & 0x00ff; // 0000000012345678

	return result;
}

void PlatformState::fillFrom(const UserFPRegsStructX86& regs) {
	x87.statusWord=regs.swd; // should be first for RIndexToSTIndex() to work
	for(std::size_t n=0;n<MAX_FPU_REG_COUNT;++n)
		x87.R[n]=edb::value80(regs.st_space,10*x87.RIndexToSTIndex(n));
	x87.controlWord=regs.cwd;
	x87.tagWord=regs.twd; // This is the true tag word, unlike in FPX regs and x86-64 FP regs structs
	x87.instPtrOffset=edb::address_t::fromZeroExtended(regs.fip);
	x87.dataPtrOffset=edb::address_t::fromZeroExtended(regs.foo);
	x87.instPtrSelector=regs.fcs;
	x87.dataPtrSelector=regs.fos;
	x87.opCode=0; // not present in the given structure
	x87.filled=true;
}
void PlatformState::fillFrom(const UserFPXRegsStructX86& regs) {
	x87.statusWord=regs.swd; // should be first for RIndexToSTIndex() to work
	for(std::size_t n=0;n<MAX_FPU_REG_COUNT;++n)
		x87.R[n]=edb::value80(regs.st_space,16*x87.RIndexToSTIndex(n));
	x87.controlWord=regs.cwd;
	x87.tagWord=x87.restoreTagWord(regs.twd);
	x87.instPtrOffset=edb::address_t::fromZeroExtended(regs.fip);
	x87.dataPtrOffset=edb::address_t::fromZeroExtended(regs.foo);
	x87.instPtrSelector=regs.fcs;
	x87.dataPtrSelector=regs.fos;
	x87.opCode=regs.fop;
	x87.filled=true;
	x87.opCodeFilled=true;
	for(std::size_t n=0;n<IA32_XMM_REG_COUNT;++n)
		avx.setXMM(n,edb::value128(regs.xmm_space,16*n));
	avx.mxcsr=regs.mxcsr;
	avx.xmmFilledIA32=true;
}
void PlatformState::fillFrom(const UserRegsStructX86_64& regs) {
	// On 32 bit OS this code would access beyond the length of the array, but it won't ever execute there
	assert(x86.GPRegs.size()==16);
	x86.GPRegs[X86::RAX] = regs.rax;
	x86.GPRegs[X86::RCX] = regs.rcx;
	x86.GPRegs[X86::RDX] = regs.rdx;
	x86.GPRegs[X86::RBX] = regs.rbx;
	x86.GPRegs[X86::RSP] = regs.rsp;
	x86.GPRegs[X86::RBP] = regs.rbp;
	x86.GPRegs[X86::RSI] = regs.rsi;
	x86.GPRegs[X86::RDI] = regs.rdi;
	x86.GPRegs[X86::R8 ] = regs.r8 ;
	x86.GPRegs[X86::R9 ] = regs.r9 ;
	x86.GPRegs[X86::R10] = regs.r10;
	x86.GPRegs[X86::R11] = regs.r11;
	x86.GPRegs[X86::R12] = regs.r12;
	x86.GPRegs[X86::R13] = regs.r13;
	x86.GPRegs[X86::R14] = regs.r14;
	x86.GPRegs[X86::R15] = regs.r15;
	x86.orig_ax = regs.orig_rax;
	x86.flags = regs.eflags;
	x86.IP = regs.rip;
	x86.segRegs[X86::ES] = regs.es;
	x86.segRegs[X86::CS] = regs.cs;
	x86.segRegs[X86::SS] = regs.ss;
	x86.segRegs[X86::DS] = regs.ds;
	x86.segRegs[X86::FS] = regs.fs;
	x86.segRegs[X86::GS] = regs.gs;
	x86.gpr32Filled=true;
	x86.gpr64Filled=true;
	if(is64Bit()) { // 32-bit processes get always zeros here, which may be wrong or meaningless
		if(x86.segRegs[X86::FS]==0) {
			x86.segRegBases[X86::FS] = regs.fs_base;
			x86.segRegBasesFilled[X86::FS]=true;
		}
		if(x86.segRegs[X86::GS]==0) {
			x86.segRegBases[X86::GS] = regs.gs_base;
			x86.segRegBasesFilled[X86::GS]=true;
		}
	}
}
void PlatformState::fillFrom(const UserFPRegsStructX86_64& regs) {
	x87.statusWord=regs.swd; // should be first for RIndexToSTIndex() to work
	for(std::size_t n=0;n<MAX_FPU_REG_COUNT;++n)
		x87.R[n]=edb::value80(regs.st_space,16*x87.RIndexToSTIndex(n));
	x87.controlWord=regs.cwd;
	x87.tagWord=x87.restoreTagWord(regs.ftw);
	x87.instPtrOffset=regs.rip;
	x87.dataPtrOffset=regs.rdp;
	x87.instPtrSelector=0;
	x87.dataPtrSelector=0;
	x87.opCode=regs.fop;
	x87.filled=true;
	x87.opCodeFilled=true;
	for(std::size_t n=0;n<MAX_XMM_REG_COUNT;++n)
		avx.setXMM(n,edb::value128(regs.xmm_space,16*n));
	avx.mxcsr=regs.mxcsr;
	avx.mxcsrMask=regs.mxcr_mask;
	avx.mxcsrMaskFilled=true;
	avx.xmmFilledIA32=true;
	avx.xmmFilledAMD64=true;
}

void PlatformState::fillFrom(const PrStatus_X86& regs)
{
	// Don't touch higher parts to avoid zeroing out bad value mark
	std::memcpy(&x86.GPRegs[X86::EAX],&regs.eax,sizeof(regs.eax));
	std::memcpy(&x86.GPRegs[X86::ECX],&regs.ecx,sizeof(regs.ecx));
	std::memcpy(&x86.GPRegs[X86::EDX],&regs.edx,sizeof(regs.edx));
	std::memcpy(&x86.GPRegs[X86::EBX],&regs.ebx,sizeof(regs.ebx));
	std::memcpy(&x86.GPRegs[X86::ESP],&regs.esp,sizeof(regs.esp));
	std::memcpy(&x86.GPRegs[X86::EBP],&regs.ebp,sizeof(regs.ebp));
	std::memcpy(&x86.GPRegs[X86::ESI],&regs.esi,sizeof(regs.esi));
	std::memcpy(&x86.GPRegs[X86::EDI],&regs.edi,sizeof(regs.edi));
	std::memcpy(&x86.orig_ax,&regs.orig_eax,sizeof(regs.orig_eax));
	std::memcpy(&x86.flags,&regs.eflags,sizeof(regs.eflags));
	std::memcpy(&x86.IP,&regs.eip,sizeof(regs.eip));
	x86.segRegs[X86::ES] = regs.es;
	x86.segRegs[X86::CS] = regs.cs;
	x86.segRegs[X86::SS] = regs.ss;
	x86.segRegs[X86::DS] = regs.ds;
	x86.segRegs[X86::FS] = regs.fs;
	x86.segRegs[X86::GS] = regs.gs;
	x86.gpr32Filled=true;
}

void PlatformState::fillFrom(const PrStatus_X86_64& regs)
{
	x86.GPRegs[X86::RAX] = regs.rax;
	x86.GPRegs[X86::RCX] = regs.rcx;
	x86.GPRegs[X86::RDX] = regs.rdx;
	x86.GPRegs[X86::RBX] = regs.rbx;
	x86.GPRegs[X86::RSP] = regs.rsp;
	x86.GPRegs[X86::RBP] = regs.rbp;
	x86.GPRegs[X86::RSI] = regs.rsi;
	x86.GPRegs[X86::RDI] = regs.rdi;
	x86.GPRegs[X86::R8 ] = regs.r8 ;
	x86.GPRegs[X86::R9 ] = regs.r9 ;
	x86.GPRegs[X86::R10] = regs.r10;
	x86.GPRegs[X86::R11] = regs.r11;
	x86.GPRegs[X86::R12] = regs.r12;
	x86.GPRegs[X86::R13] = regs.r13;
	x86.GPRegs[X86::R14] = regs.r14;
	x86.GPRegs[X86::R15] = regs.r15;
	x86.orig_ax = regs.orig_rax;
	x86.flags = regs.rflags;
	x86.IP = regs.rip;
	x86.segRegs[X86::ES] = regs.es;
	x86.segRegs[X86::CS] = regs.cs;
	x86.segRegs[X86::SS] = regs.ss;
	x86.segRegs[X86::DS] = regs.ds;
	x86.segRegs[X86::FS] = regs.fs;
	x86.segRegs[X86::GS] = regs.gs;
	x86.gpr32Filled=true;
	x86.gpr64Filled=true;
	x86.segRegBases[X86::FS] = regs.fs_base;
	x86.segRegBasesFilled[X86::FS]=true;
	x86.segRegBases[X86::GS] = regs.gs_base;
	x86.segRegBasesFilled[X86::GS]=true;
}

void PlatformState::fillFrom(const X86XState& regs, std::size_t sizeFromKernel) {
	if(sizeFromKernel<X86XState::AVX_SIZE) {
		// Shouldn't ever happen. If AVX isn't supported, the ptrace call will fail.
		qDebug() << "Size of X86_XSTATE returned from the kernel appears less than expected";
		return;
	}

	avx.xcr0=regs.xcr0;

	bool statePresentX87=regs.xstate_bv & X86XState::FEATURE_X87;
	bool statePresentSSE=regs.xstate_bv & X86XState::FEATURE_SSE;
	bool statePresentAVX=regs.xstate_bv & X86XState::FEATURE_AVX;

	// Due to the lazy saving the feature bits may be unset in XSTATE_BV if the app
	// has not touched the corresponding registers yet. But once the registers are
	// touched, they are initialized to zero by the OS (not control/tag ones). To the app
	// it looks as if the registers have always been zero. Thus we should provide the same
	// illusion to the user.
	if(statePresentX87) {
		x87.statusWord=regs.swd; // should be first for RIndexToSTIndex() to work
		for(std::size_t n=0;n<MAX_FPU_REG_COUNT;++n)
			x87.R[n]=edb::value80(regs.st_space,16*x87.RIndexToSTIndex(n));
		x87.controlWord=regs.cwd;
		x87.tagWord=x87.restoreTagWord(regs.twd);
		x87.instPtrOffset=regs.fioff;
		x87.dataPtrOffset=regs.fooff;
		if(is64Bit()) {
			std::memcpy(reinterpret_cast<char*>(&x87.instPtrOffset)+4,&regs.fiseg,sizeof regs.fiseg);
			std::memcpy(reinterpret_cast<char*>(&x87.dataPtrOffset)+4,&regs.foseg,sizeof regs.foseg);
			x87.instPtrSelector=0;
			x87.dataPtrSelector=0;
		}
		else {
			x87.instPtrSelector=regs.fiseg;
			x87.dataPtrSelector=regs.foseg;
		}
		x87.opCode=regs.fop;
		x87.filled=true;
		x87.opCodeFilled=true;
	} else {
		std::memset(&x87,0,sizeof x87);
		x87.controlWord=regs.cwd; // this appears always present
		x87.tagWord=0xffff;
		x87.filled=true;
		x87.opCodeFilled=true;
	}
	if(statePresentAVX) {
		for(std::size_t n=0;n<MAX_YMM_REG_COUNT;++n)
			avx.setYMM(n,edb::value128(regs.xmm_space,16*n),edb::value128(regs.ymmh_space,16*n));
		avx.mxcsr=regs.mxcsr;
		avx.mxcsrMask=regs.mxcsr_mask;
		avx.mxcsrMaskFilled=true;
		avx.xmmFilledIA32=true;
		avx.xmmFilledAMD64=true;
		avx.ymmFilled=true;
	} else if(statePresentSSE) {
		// If AVX state management has been enabled by the OS,
		// the state may be not present due to lazy saving,
		// so initialize the space with zeros
		if(avx.xcr0 & X86XState::FEATURE_AVX) {
			for(std::size_t n=0;n<MAX_YMM_REG_COUNT;++n)
				avx.setYMM(n,edb::value256::fromZeroExtended(0));
			avx.ymmFilled=true;
		}
		// Now we can fill in the XMM registers
		for(std::size_t n=0;n<MAX_XMM_REG_COUNT;++n)
			avx.setXMM(n,edb::value128(regs.xmm_space,16*n));
		avx.mxcsr=regs.mxcsr;
		avx.mxcsrMask=regs.mxcsr_mask;
		avx.mxcsrMaskFilled=true;
		avx.xmmFilledIA32=true;
		avx.xmmFilledAMD64=true;
	} else {
		avx.mxcsr=regs.mxcsr;
		avx.mxcsrMask=regs.mxcsr_mask;
		avx.mxcsrMaskFilled=true;
		// Only fill the registers which are actually supported, leave invalidity marks intact for other parts
		if(avx.xcr0 & X86XState::FEATURE_AVX) { // If AVX state management has been enabled by the OS
			for(std::size_t n=0;n<MAX_YMM_REG_COUNT;++n)
				avx.setYMM(n,edb::value256::fromZeroExtended(0));
			avx.xmmFilledIA32=true;
			avx.xmmFilledAMD64=true;
			avx.ymmFilled=true;
		} else if(avx.xcr0 & X86XState::FEATURE_SSE) { // If SSE state management has been enabled by the OS
			for(std::size_t n=0;n<MAX_YMM_REG_COUNT;++n)
				avx.setYMM(n,edb::value256::fromZeroExtended(0));
			avx.xmmFilledIA32=true;
			avx.xmmFilledAMD64=true;
		}
	}
}

void PlatformState::fillStruct(UserRegsStructX86& regs) const
{
	util::markMemory(&regs,sizeof(regs));
	if(x86.gpr32Filled) {
		regs.eax=x86.GPRegs[X86::EAX];
		regs.ecx=x86.GPRegs[X86::ECX];
		regs.edx=x86.GPRegs[X86::EDX];
		regs.ebx=x86.GPRegs[X86::EBX];
		regs.esp=x86.GPRegs[X86::ESP];
		regs.ebp=x86.GPRegs[X86::EBP];
		regs.esi=x86.GPRegs[X86::ESI];
		regs.edi=x86.GPRegs[X86::EDI];
		regs.xes=x86.segRegs[X86::ES];
		regs.xcs=x86.segRegs[X86::CS];
		regs.xss=x86.segRegs[X86::SS];
		regs.xds=x86.segRegs[X86::DS];
		regs.xfs=x86.segRegs[X86::FS];
		regs.xgs=x86.segRegs[X86::GS];
		regs.orig_eax=x86.orig_ax;
		regs.eflags=x86.flags;
		regs.eip=x86.IP;
	}
}
void PlatformState::fillStruct(UserRegsStructX86_64& regs) const
{
	// If 64-bit part is not filled in state, we'll set marked values
	if(x86.gpr64Filled || x86.gpr32Filled) {
		regs.rax=x86.GPRegs[X86::RAX];
		regs.rcx=x86.GPRegs[X86::RCX];
		regs.rdx=x86.GPRegs[X86::RDX];
		regs.rbx=x86.GPRegs[X86::RBX];
		regs.rsp=x86.GPRegs[X86::RSP];
		regs.rbp=x86.GPRegs[X86::RBP];
		regs.rsi=x86.GPRegs[X86::RSI];
		regs.rdi=x86.GPRegs[X86::RDI];
		regs.r8 =x86.GPRegs[X86::R8 ];
		regs.r9 =x86.GPRegs[X86::R9 ];
		regs.r10=x86.GPRegs[X86::R10];
		regs.r11=x86.GPRegs[X86::R11];
		regs.r12=x86.GPRegs[X86::R12];
		regs.r13=x86.GPRegs[X86::R13];
		regs.r14=x86.GPRegs[X86::R14];
		regs.r15=x86.GPRegs[X86::R15];
		regs.es=x86.segRegs[X86::ES];
		regs.cs=x86.segRegs[X86::CS];
		regs.ss=x86.segRegs[X86::SS];
		regs.ds=x86.segRegs[X86::DS];
		regs.fs=x86.segRegs[X86::FS];
		regs.gs=x86.segRegs[X86::GS];
		regs.fs_base=x86.segRegBases[X86::FS];
		regs.gs_base=x86.segRegBases[X86::GS];
		regs.orig_rax=x86.orig_ax;
		regs.eflags=x86.flags;
		regs.rip=x86.IP;
	}
}

void PlatformState::fillStruct(PrStatus_X86_64& regs) const
{
	// If 64-bit part is not filled in state, we'll set marked values
	if(x86.gpr64Filled || x86.gpr32Filled) {
		regs.rax=x86.GPRegs[X86::RAX];
		regs.rcx=x86.GPRegs[X86::RCX];
		regs.rdx=x86.GPRegs[X86::RDX];
		regs.rbx=x86.GPRegs[X86::RBX];
		regs.rsp=x86.GPRegs[X86::RSP];
		regs.rbp=x86.GPRegs[X86::RBP];
		regs.rsi=x86.GPRegs[X86::RSI];
		regs.rdi=x86.GPRegs[X86::RDI];
		regs.r8 =x86.GPRegs[X86::R8 ];
		regs.r9 =x86.GPRegs[X86::R9 ];
		regs.r10=x86.GPRegs[X86::R10];
		regs.r11=x86.GPRegs[X86::R11];
		regs.r12=x86.GPRegs[X86::R12];
		regs.r13=x86.GPRegs[X86::R13];
		regs.r14=x86.GPRegs[X86::R14];
		regs.r15=x86.GPRegs[X86::R15];
		regs.orig_rax=x86.orig_ax;
		regs.rflags=x86.flags;
		regs.rip=x86.IP;
		regs.es=x86.segRegs[X86::ES];
		regs.cs=x86.segRegs[X86::CS];
		regs.ss=x86.segRegs[X86::SS];
		regs.ds=x86.segRegs[X86::DS];
		regs.fs=x86.segRegs[X86::FS];
		regs.gs=x86.segRegs[X86::GS];
		regs.fs_base=x86.segRegBases[X86::FS];
		regs.gs_base=x86.segRegBases[X86::GS];
	}
}

void PlatformState::fillStruct(UserFPRegsStructX86& regs) const {
	util::markMemory(&regs,sizeof(regs));
	if(x87.filled) {
		regs.swd=x87.statusWord;
		regs.cwd=x87.controlWord;
		regs.twd=x87.tagWord;
		regs.fip=x87.instPtrOffset;
		regs.foo=x87.dataPtrOffset;
		regs.fcs=x87.instPtrSelector;
		regs.fos=x87.dataPtrSelector;
		for(std::size_t n=0;n<MAX_FPU_REG_COUNT;++n)
			std::memcpy(reinterpret_cast<char*>(regs.st_space)+10*x87.RIndexToSTIndex(n),&x87.R[n],sizeof(x87.R[n]));
	}
}

void PlatformState::fillStruct(UserFPRegsStructX86_64& regs) const {
	util::markMemory(&regs,sizeof(regs));
	if(x87.filled) {
		regs.swd=x87.statusWord;
		regs.cwd=x87.controlWord;
		regs.ftw=x87.reducedTagWord();
		regs.rip=x87.instPtrOffset;
		regs.rdp=x87.dataPtrOffset;
		if(x87.opCodeFilled)
			regs.fop=x87.opCode;
		for(std::size_t n=0;n<MAX_FPU_REG_COUNT;++n)
			std::memcpy(reinterpret_cast<char*>(regs.st_space)+16*x87.RIndexToSTIndex(n),&x87.R[n],sizeof(x87.R[n]));
		if(avx.xmmFilledIA32||avx.xmmFilledAMD64) {
			for(std::size_t n=0;n<MAX_XMM_REG_COUNT;++n)
				std::memcpy(reinterpret_cast<char*>(regs.xmm_space)+16*n,&avx.zmmStorage[n],sizeof(edb::value128));
			regs.mxcsr=avx.mxcsr;
		}
		if(avx.mxcsrMaskFilled)
			regs.mxcr_mask=avx.mxcsrMask;

	}
}

void PlatformState::fillStruct(UserFPXRegsStructX86& regs) const {
	util::markMemory(&regs,sizeof regs);
	if(x87.filled) {
		regs.swd=x87.statusWord;
		regs.twd=x87.reducedTagWord();
		regs.cwd=x87.controlWord;
		regs.fip=x87.instPtrOffset;
		regs.foo=x87.dataPtrOffset;
		regs.fcs=x87.instPtrSelector;
		regs.fos=x87.dataPtrSelector;
		regs.fop=x87.opCode;
		for(std::size_t n=0;n<MAX_FPU_REG_COUNT;++n)
			std::memcpy(reinterpret_cast<char*>(&regs.st_space)+16*x87.RIndexToSTIndex(n),&x87.R[n],sizeof x87.R[n]);
	}
	if(avx.xmmFilledIA32) {
		regs.mxcsr=avx.mxcsr;
		for(std::size_t n=0;n<IA32_XMM_REG_COUNT;++n)
			std::memcpy(reinterpret_cast<char*>(&regs.xmm_space)+16*n,&avx.zmmStorage[n],sizeof(edb::value128));
	}
}

size_t PlatformState::fillStruct(X86XState& regs) const {
	util::markMemory(&regs,sizeof regs);
	// Zero out reserved bytes; set xstate_bv to 0
	std::memset(regs.xstate_hdr_bytes,0,sizeof regs.xstate_hdr_bytes);

	regs.xcr0=avx.xcr0;
	if(x87.filled) {
		regs.swd=x87.statusWord;
		regs.cwd=x87.controlWord;
		regs.twd=x87.reducedTagWord();
		regs.fioff=x87.instPtrOffset;
		regs.fooff=x87.dataPtrOffset;
		regs.fiseg=x87.instPtrSelector;
		regs.foseg=x87.dataPtrSelector;
		regs.fop=x87.opCode;
		for(size_t n=0;n<MAX_FPU_REG_COUNT;++n)
			std::memcpy(reinterpret_cast<char*>(&regs.st_space)+16*x87.RIndexToSTIndex(n),&x87.R[n],sizeof x87.R[n]);

		regs.xstate_bv|=X86XState::FEATURE_X87;
	}
	if(avx.xmmFilledIA32) {
		const auto nMax= avx.xmmFilledAMD64 ? AMD64_XMM_REG_COUNT : IA32_XMM_REG_COUNT;
		for(size_t n=0;n<nMax;++n)
			std::memcpy(reinterpret_cast<char*>(&regs.xmm_space)+16*n,
						&avx.zmmStorage[n],sizeof(edb::value128));
		regs.mxcsr=avx.mxcsr;
		regs.mxcsr_mask=avx.mxcsrMask;
		regs.xstate_bv|=X86XState::FEATURE_SSE;
	}
	if(avx.ymmFilled) {
		// In this case SSE state is already filled by the code writing XMMx&MXCSR registers
		for(size_t n=0;n<MAX_YMM_REG_COUNT;++n)
			std::memcpy(reinterpret_cast<char*>(&regs.ymmh_space)+16*n,
						reinterpret_cast<const char*>(&avx.zmmStorage[n])+sizeof(edb::value128),
						sizeof(edb::value128));
		regs.xstate_bv|=X86XState::FEATURE_AVX;
	}

	size_t size;
	if(regs.xstate_bv&X86XState::FEATURE_AVX)
		size=X86XState::AVX_SIZE;
	else if(regs.xstate_bv&(X86XState::FEATURE_X87|X86XState::FEATURE_SSE))
		size=X86XState::SSE_SIZE; // minimum size: legacy state + xsave header
	else
		size=0;
	return size;
}

edb::value128 PlatformState::AVX::xmm(std::size_t index) const {
	return edb::value128(zmmStorage[index]);
}

edb::value256 PlatformState::AVX::ymm(std::size_t index) const {
	return edb::value256(zmmStorage[index]);
}

edb::value512 PlatformState::AVX::zmm(std::size_t index) const {
	return zmmStorage[index];
}

void PlatformState::AVX::setXMM(std::size_t index, edb::value128 value) {
	// leave upper part unchanged.
	std::memcpy(&zmmStorage[index],&value,sizeof value);
}

void PlatformState::AVX::setYMM(std::size_t index, edb::value128 low, edb::value128 high) {
	// leave upper part unchanged.
	std::memcpy(reinterpret_cast<uint8_t*>(&zmmStorage[index])+0,&low,sizeof low);
	std::memcpy(reinterpret_cast<uint8_t*>(&zmmStorage[index])+16,&high,sizeof high);
}

void PlatformState::AVX::setYMM(std::size_t index, edb::value256 value) {
	// leave upper part unchanged.
	std::memcpy(&zmmStorage[index],&value,sizeof value);
}

void PlatformState::AVX::setZMM(std::size_t index, edb::value512 value) {
	zmmStorage[index]=value;
}

void PlatformState::X86::clear() {
	util::markMemory(this,sizeof(*this));
	gpr32Filled=false;
	gpr64Filled=false;
	for(auto& base : segRegBasesFilled)
		base=false;
}

bool PlatformState::X86::empty() const {
	return !gpr32Filled;
}

void PlatformState::X87::clear() {
	util::markMemory(this,sizeof(*this));
	filled=false;
	opCodeFilled=false;
}

bool PlatformState::X87::empty() const {
	return !filled;
}

void PlatformState::AVX::clear() {
	util::markMemory(this,sizeof(*this));
	xmmFilledIA32=false;
	xmmFilledAMD64=false;
	ymmFilled=false;
	zmmFilled=false;
}

bool PlatformState::AVX::empty() const {
	return !xmmFilledIA32;
}

//------------------------------------------------------------------------------
// Name: PlatformState
// Desc:
//------------------------------------------------------------------------------
PlatformState::PlatformState() {
	clear();
}

//------------------------------------------------------------------------------
// Name: PlatformState::clone
// Desc: makes a copy of the state object
//------------------------------------------------------------------------------
IState *PlatformState::clone() const {
	return new PlatformState(*this);
}

//------------------------------------------------------------------------------
// Name: flags_to_string
// Desc: returns the flags in a string form appropriate for this platform
//------------------------------------------------------------------------------
QString PlatformState::flags_to_string(edb::reg_t flags) const {
	char buf[32];
	qsnprintf(
		buf,
		sizeof(buf),
		"%c %c %c %c %c %c %c %c %c",
		((flags & 0x001) ? 'C' : 'c'),
		((flags & 0x004) ? 'P' : 'p'),
		((flags & 0x010) ? 'A' : 'a'),
		((flags & 0x040) ? 'Z' : 'z'),
		((flags & 0x080) ? 'S' : 's'),
		((flags & 0x100) ? 'T' : 't'),
		((flags & 0x200) ? 'I' : 'i'),
		((flags & 0x400) ? 'D' : 'd'),
		((flags & 0x800) ? 'O' : 'o'));
	return buf;
}

//------------------------------------------------------------------------------
// Name: flags_to_string
// Desc: returns the flags in a string form appropriate for this platform
//------------------------------------------------------------------------------
QString PlatformState::flags_to_string() const {
	return flags_to_string(flags());
}

template<std::size_t bitSize=0, typename Names, typename Regs>
Register findRegisterValue(const Names& names, const Regs& regs, const QString& regName, Register::Type type, size_t maxNames, int shift=0)
{
	const auto end=names.begin()+maxNames;
	auto regNameFoundIter=std::find(names.begin(),end,regName);
	if(regNameFoundIter!=end)
		return make_Register<bitSize>(regName, regs[regNameFoundIter-names.begin()]>>shift, type);
	else
		return Register();
}

//------------------------------------------------------------------------------
// Name: value
// Desc: returns a Register object which represents the register with the name
//       supplied
//------------------------------------------------------------------------------
Register PlatformState::value(const QString &reg) const {
	const QString regName = reg.toLower();

	Register found;
	if(x86.gpr32Filled) // don't return valid Register with garbage value
	{
		if(reg==x86.origRAXName && x86.gpr64Filled && is64Bit())
			return make_Register<64>(x86.origRAXName, x86.orig_ax, Register::TYPE_GPR);

		if(reg==x86.origEAXName)
			return make_Register<32>(x86.origEAXName, x86.orig_ax, Register::TYPE_GPR);


		if(x86.gpr64Filled && is64Bit() && !!(found=findRegisterValue(x86.GPReg64Names, x86.GPRegs, regName, Register::TYPE_GPR, gpr64_count())))
			return found;
		if(!!(found=findRegisterValue<32>(x86.GPReg32Names, x86.GPRegs, regName, Register::TYPE_GPR, gpr_count())))
			return found;
		if(!!(found=findRegisterValue<16>(x86.GPReg16Names, x86.GPRegs, regName, Register::TYPE_GPR, gpr_count())))
			return found;
		if(!!(found=findRegisterValue<8>(x86.GPReg8LNames, x86.GPRegs, regName, Register::TYPE_GPR, gpr_low_addressable_count())))
			return found;
		if(!!(found=findRegisterValue<8>(x86.GPReg8HNames, x86.GPRegs, regName, Register::TYPE_GPR, gpr_high_addressable_count(), 8)))
			return found;
		if(!!(found=findRegisterValue(x86.segRegNames, x86.segRegs, regName, Register::TYPE_SEG, seg_reg_count())))
			return found;
		if(regName.mid(1)=="s_base") {
			const QString segRegName=regName.mid(0,2);
			const auto end=x86.segRegNames.end();
			const auto regNameFoundIter=std::find(x86.segRegNames.begin(),end,segRegName);
			if(regNameFoundIter!=end) {
				const size_t index=regNameFoundIter-x86.segRegNames.begin();
				if(!x86.segRegBasesFilled[index])
					return Register();
				const auto value=x86.segRegBases[index];
				if(is64Bit())
					return make_Register(regName, value, Register::TYPE_SEG);
				else
					return make_Register<32>(regName, value, Register::TYPE_SEG);
			}
		}

		if(is64Bit() && regName==x86.flags64Name)
			return make_Register(x86.flags64Name, x86.flags, Register::TYPE_COND);
		if(regName==x86.flags32Name)
			return make_Register<32>(x86.flags32Name, x86.flags, Register::TYPE_COND);
		if(regName==x86.flags16Name)
			return make_Register<16>(x86.flags16Name, x86.flags, Register::TYPE_COND);

		if(is64Bit() && regName==x86.IP64Name)
			return make_Register(x86.IP64Name, x86.IP, Register::TYPE_IP);
		if(regName==x86.IP32Name)
			return make_Register<32>(x86.IP32Name, x86.IP, Register::TYPE_IP);
		if(regName==x86.IP16Name)
			return make_Register<16>(x86.IP16Name, x86.IP, Register::TYPE_IP);
	}
	if(x87.filled) {
		QRegExp Rx("^r([0-7])$");
		if(Rx.indexIn(regName)!=-1) {
			QChar digit=Rx.cap(1).at(0);
			assert(digit.isDigit());
			char digitChar=digit.toLatin1();
			std::size_t i=digitChar-'0';
			assert(fpuIndexValid(i));
			return make_Register(regName, x87.R[i], Register::TYPE_FPU);
		}
	}
	if(x87.filled) {
		QRegExp STx("^st\\(?([0-7])\\)?$");
		if(STx.indexIn(regName)!=-1) {
			QChar digit=STx.cap(1).at(0);
			assert(digit.isDigit());
			char digitChar=digit.toLatin1();
			std::size_t i=digitChar-'0';
			assert(fpuIndexValid(i));
			return make_Register(regName, x87.st(i), Register::TYPE_FPU);
		}
	}
	if(x87.filled) {
		if(regName=="fip"||regName=="fdp") {
			const edb::address_t addr = regName=="fip" ? x87.instPtrOffset : x87.dataPtrOffset;
			if(is64Bit())
				return make_Register<64>(regName,addr,Register::TYPE_FPU);
			else
				return make_Register<32>(regName,addr,Register::TYPE_FPU);
		}
		if(regName=="fis"||regName=="fds") {
			const edb::value16 val = regName=="fis" ? x87.instPtrSelector : x87.dataPtrSelector;
			return make_Register<16>(regName,val,Register::TYPE_FPU);
		}
		if(regName=="fopcode") {
			return make_Register<16>(regName,x87.opCode,Register::TYPE_FPU);
		}
	}
	if(x87.filled) {
		QRegExp MMx("^mm([0-7])$");
		if(MMx.indexIn(regName)!=-1) {
			QChar digit=MMx.cap(1).at(0);
			assert(digit.isDigit());
			char digitChar=digit.toLatin1();
			std::size_t i=digitChar-'0';
			assert(mmxIndexValid(i));
			return make_Register(regName, x87.R[i].mantissa(), Register::TYPE_SIMD);
		}
	}
	if(avx.xmmFilledIA32) {
		QRegExp XMMx("^xmm([0-9]|1[0-5])$");
		if(XMMx.indexIn(regName)!=-1) {
			bool ok=false;
			std::size_t i=XMMx.cap(1).toUShort(&ok);
			assert(ok);
			if(i>=IA32_XMM_REG_COUNT && !avx.xmmFilledAMD64)
				return Register();
			if(xmmIndexValid(i)) // May be invalid but legitimate for a disassembler: e.g. XMM13 but 32 bit mode
				return make_Register(regName, avx.xmm(i), Register::TYPE_SIMD);
		}
	}
	if(avx.ymmFilled) {
		QRegExp YMMx("^ymm([0-9]|1[0-5])$");
		if(YMMx.indexIn(regName)!=-1) {
			bool ok=false;
			std::size_t i=YMMx.cap(1).toUShort(&ok);
			assert(ok);
			if(ymmIndexValid(i)) // May be invalid but legitimate for a disassembler: e.g. YMM13 but 32 bit mode
				return make_Register(regName, avx.ymm(i), Register::TYPE_SIMD);
		}
	}
	if(avx.xmmFilledIA32 && regName==avx.mxcsrName)
		return make_Register(avx.mxcsrName, avx.mxcsr, Register::TYPE_COND);
	return Register();
}

//------------------------------------------------------------------------------
// Name: instruction_pointer_register
// Desc:
//------------------------------------------------------------------------------
Register PlatformState::instruction_pointer_register() const {
	if(x86.gpr64Filled && is64Bit())
		return make_Register(x86.IP64Name, x86.IP, Register::TYPE_GPR);
	else if(x86.gpr32Filled)
		return make_Register<32>(x86.IP32Name, x86.IP, Register::TYPE_GPR);
	return Register();
}

//------------------------------------------------------------------------------
// Name: frame_pointer
// Desc: returns what is conceptually the frame pointer for this platform
//------------------------------------------------------------------------------
edb::address_t PlatformState::frame_pointer() const {
	return stack_pointer();
}

//------------------------------------------------------------------------------
// Name: instruction_pointer
// Desc: returns the instruction pointer for this platform
//------------------------------------------------------------------------------
edb::address_t PlatformState::instruction_pointer() const {
	return instruction_pointer_register().valueAsAddress();
}

//------------------------------------------------------------------------------
// Name: stack_pointer
// Desc: returns the stack pointer for this platform
//------------------------------------------------------------------------------
edb::address_t PlatformState::stack_pointer() const {
	return gp_register(X86::RSP).valueAsAddress();
}

//------------------------------------------------------------------------------
// Name: debug_register
// Desc:
//------------------------------------------------------------------------------
edb::reg_t PlatformState::debug_register(size_t n) const {
	assert(dbgIndexValid(n));
	return x86.dbgRegs[n];
}

//------------------------------------------------------------------------------
// Name: flags_register
// Desc:
//------------------------------------------------------------------------------
Register PlatformState::flags_register() const {
	if(x86.gpr64Filled && is64Bit())
		return make_Register(x86.flags64Name, x86.flags, Register::TYPE_GPR);
	else if(x86.gpr32Filled)
		return make_Register<32>(x86.flags32Name, x86.flags, Register::TYPE_GPR);
	return Register();
}

//------------------------------------------------------------------------------
// Name: flags
// Desc:
//------------------------------------------------------------------------------
edb::reg_t PlatformState::flags() const {
	return flags_register().valueAsInteger();
}

//------------------------------------------------------------------------------
// Name: fpu_stack_pointer
// Desc:
//------------------------------------------------------------------------------
int PlatformState::fpu_stack_pointer() const {
	return x87.stackPointer();
}

//------------------------------------------------------------------------------
// Name: fpu_register
// Desc:
//------------------------------------------------------------------------------
edb::value80 PlatformState::fpu_register(size_t n) const {
	assert(fpuIndexValid(n));
	return x87.R[n];
}

//------------------------------------------------------------------------------
// Name: fpu_register_is_empty
// Desc: Returns true if Rn register is empty when treated in terms of FPU stack
//------------------------------------------------------------------------------
bool PlatformState::fpu_register_is_empty(std::size_t n) const {
	return x87.tag(n)==X87::TAG_EMPTY;
}

//------------------------------------------------------------------------------
// Name: fpu_register_tag_string
// Desc:
//------------------------------------------------------------------------------
QString PlatformState::fpu_register_tag_string(std::size_t n) const {
	int tag=x87.tag(n);
	static const std::unordered_map<int,QString>
				names{{X87::TAG_VALID,  "Valid"},
					  {X87::TAG_ZERO,   "Zero"},
					  {X87::TAG_SPECIAL,"Special"},
					  {X87::TAG_EMPTY,  "Empty"}};
	return names.at(tag);
}

edb::value16 PlatformState::fpu_control_word() const {
	return x87.controlWord;
}

edb::value16 PlatformState::fpu_status_word() const {
	return x87.statusWord;
}

edb::value16 PlatformState::fpu_tag_word() const {
	return x87.tagWord;
}

//------------------------------------------------------------------------------
// Name: adjust_stack
// Desc:
//------------------------------------------------------------------------------
void PlatformState::adjust_stack(int bytes) {
	x86.GPRegs[X86::RSP] += bytes;
}

//------------------------------------------------------------------------------
// Name: clear
// Desc:
//------------------------------------------------------------------------------
void PlatformState::clear() {
	x86.clear();
	x87.clear();
	avx.clear();
}

//------------------------------------------------------------------------------
// Name: empty
// Desc:
//------------------------------------------------------------------------------
bool PlatformState::empty() const {
	return x86.empty() && x87.empty() && avx.empty();
}

//------------------------------------------------------------------------------
// Name: set_debug_register
// Desc:
//------------------------------------------------------------------------------
void PlatformState::set_debug_register(size_t n, edb::reg_t value) {
	assert(dbgIndexValid(n));
	x86.dbgRegs[n] = value;
}

//------------------------------------------------------------------------------
// Name: set_flags
// Desc:
//------------------------------------------------------------------------------
void PlatformState::set_flags(edb::reg_t flags) {
	x86.flags=flags;
}

//------------------------------------------------------------------------------
// Name: set_instruction_pointer
// Desc:
//------------------------------------------------------------------------------
void PlatformState::set_instruction_pointer(edb::address_t value) {
	x86.IP=value;
	x86.orig_ax=-1;
}

//------------------------------------------------------------------------------
// Name: gp_register
// Desc:
//------------------------------------------------------------------------------
Register PlatformState::gp_register(size_t n) const {

	if(gprIndexValid(n)) {

		if(x86.gpr64Filled && is64Bit())
			return make_Register(x86.GPReg64Names[n], x86.GPRegs[n], Register::TYPE_GPR);
		else if(x86.gpr32Filled && n<IA32_GPR_COUNT)
			return make_Register<32>(x86.GPReg32Names[n], x86.GPRegs[n], Register::TYPE_GPR);
	}
	return Register();
}

//------------------------------------------------------------------------------
// Name: set_register
// Desc:
//------------------------------------------------------------------------------
void PlatformState::set_register(const Register& reg) {
	const QString regName = reg.name().toLower();

	const auto gpr_end=GPRegNames().begin()+gpr_count();
	auto GPRegNameFoundIter=std::find(GPRegNames().begin(), gpr_end, regName);
	if(GPRegNameFoundIter!=gpr_end)
	{
		std::size_t index=GPRegNameFoundIter-GPRegNames().begin();
		x86.GPRegs[index]=reg.value<edb::value64>();
		return;
	}
	auto segRegNameFoundIter=std::find(x86.segRegNames.begin(), x86.segRegNames.end(), regName);
	if(segRegNameFoundIter!=x86.segRegNames.end())
	{
		std::size_t index=segRegNameFoundIter-x86.segRegNames.begin();
		x86.segRegs[index]=reg.value<edb::seg_reg_t>();
		return;
	}
	if(regName==IPName())
	{
		x86.IP=reg.value<edb::value64>();
		return;
	}
	if(regName==flagsName())
	{
		x86.flags=reg.value<edb::value64>();
		return;
	}
	if(regName==avx.mxcsrName)
	{
		avx.mxcsr=reg.value<edb::value32>();
		return;
	}
	{
		QRegExp MMx("^mm([0-7])$");
		if(MMx.indexIn(regName)!=-1) {
			QChar digit=MMx.cap(1).at(0);
			assert(digit.isDigit());
			char digitChar=digit.toLatin1();
			std::size_t i=digitChar-'0';
			assert(mmxIndexValid(i));
			const auto value=reg.value<edb::value64>();
			std::memcpy(&x87.R[i],&value,sizeof value);
			const uint16_t RiUpper=0xffff;
			std::memcpy(reinterpret_cast<char*>(&x87.R[i])+sizeof value,&RiUpper,sizeof RiUpper);
			return;
		}
	}
	{
		QRegExp Rx("^r([0-7])$");
		if(Rx.indexIn(regName)!=-1) {
			QChar digit=Rx.cap(1).at(0);
			assert(digit.isDigit());
			char digitChar=digit.toLatin1();
			std::size_t i=digitChar-'0';
			assert(fpuIndexValid(i));
			const auto value=reg.value<edb::value80>();
			std::memcpy(&x87.R[i],&value,sizeof value);
			return;
		}
	}
	{
		QRegExp Rx("^st\\(?([0-7])\\)?$");
		if(Rx.indexIn(regName)!=-1) {
			QChar digit=Rx.cap(1).at(0);
			assert(digit.isDigit());
			char digitChar=digit.toLatin1();
			std::size_t i=digitChar-'0';
			assert(fpuIndexValid(i));
			const auto value=reg.value<edb::value80>();
			std::memcpy(&x87.st(i),&value,sizeof value);
			return;
		}
	}
	{
		QRegExp XMMx("^xmm([12]?[0-9]|3[01])$");
		if(XMMx.indexIn(regName)!=-1) {
			const auto value=reg.value<edb::value128>();
			bool indexReadOK=false;
			std::size_t i=XMMx.cap(1).toInt(&indexReadOK);
			assert(indexReadOK && xmmIndexValid(i));
			std::memcpy(&avx.zmmStorage[i],&value,sizeof value);
			return;
		}
	}
	{
		QRegExp YMMx("^ymm([12]?[0-9]|3[01])$");
		if(YMMx.indexIn(regName)!=-1) {
			const auto value=reg.value<edb::value256>();
			bool indexReadOK=false;
			std::size_t i=YMMx.cap(1).toInt(&indexReadOK);
			assert(indexReadOK && ymmIndexValid(i));
			std::memcpy(&avx.zmmStorage[i],&value,sizeof value);
			return;
		}
	}
	qDebug().nospace() << "fixme: set_register(0x"<< qPrintable(reg.toHexString()) <<"): didn't set register " << reg.name();
}

//------------------------------------------------------------------------------
// Name: set_register
// Desc:
//------------------------------------------------------------------------------
void PlatformState::set_register(const QString &name, edb::reg_t value) {

	const QString regName = name.toLower();
	set_register(make_Register<64>(regName,value,Register::TYPE_GPR));
}

//------------------------------------------------------------------------------
// Name: mmx_register
// Desc:
//------------------------------------------------------------------------------
Register PlatformState::mmx_register(size_t n) const {
	if(!mmxIndexValid(n))
		return Register();
	edb::value64 value(x87.R[n].mantissa());
	return make_Register(QString("mm%1").arg(n),value,Register::TYPE_SIMD);
}

//------------------------------------------------------------------------------
// Name: xmm_register
// Desc:
//------------------------------------------------------------------------------
Register PlatformState::xmm_register(size_t n) const {
	if(!xmmIndexValid(n) || !avx.xmmFilledIA32)
		return Register();
	if(n>=IA32_XMM_REG_COUNT && !avx.xmmFilledAMD64)
		return Register();
	edb::value128 value(avx.xmm(n));
	return make_Register(QString("xmm%1").arg(n),value,Register::TYPE_SIMD);
}

//------------------------------------------------------------------------------
// Name: ymm_register
// Desc:
//------------------------------------------------------------------------------
Register PlatformState::ymm_register(size_t n) const {
	if(!ymmIndexValid(n) || !avx.ymmFilled)
		return Register();
	edb::value256 value(avx.ymm(n));
	return make_Register(QString("ymm%1").arg(n),value,Register::TYPE_SIMD);
}

}
