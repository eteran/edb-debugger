/*
Copyright (C) 2006 - 2014 Evan Teran
                          eteran@alum.rit.edu

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
#include <unordered_map>

namespace DebuggerCore {

const std::array<const char*,GPR_COUNT> PlatformState::X86::GPRegNames={
#ifdef EDB_X86
	"eax",
	"ecx",
	"edx",
	"ebx",
	"esp",
	"ebp",
	"esi",
	"edi"
#elif defined EDB_X86_64
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
#endif
};
const std::array<const char*,GPR_COUNT> PlatformState::X86::GPReg32Names=
#ifdef EDB_X86
	GPRegNames;
#elif defined EDB_X86_64
   {"eax",
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
	"r15d"};
#endif

const std::array<const char*,GPR_COUNT> PlatformState::X86::GPReg16Names={
	"ax",
	"cx",
	"dx",
	"bx",
	"sp",
	"bp",
	"si",
	"di"
#if defined EDB_X86_64
   ,"r8w",
	"r9w",
	"r10w",
	"r11w",
	"r12w",
	"r13w",
	"r14w",
	"r15w"
#endif
};
const std::array<const char*,GPR_LOW_ADDRESSABLE_COUNT> PlatformState::X86::GPReg8LNames={
	"al",
	"cl",
	"dl",
	"bl",
#if defined EDB_X86_64
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
#endif
};
const std::array<const char*,GPR_HIGH_ADDRESSABLE_COUNT> PlatformState::X86::GPReg8HNames={
	"ah",
	"ch",
	"dh",
	"bh"
};
const std::array<const char*,SEG_REG_COUNT> PlatformState::X86::segRegNames={
	"es",
	"cs",
	"ss",
	"ds",
	"fs",
	"gs"
};

void PlatformState::fillFrom(const UserRegsStructX86& regs) {
	x86.GPRegs[X86::EAX] = regs.eax;
	x86.GPRegs[X86::ECX] = regs.ecx;
	x86.GPRegs[X86::EDX] = regs.edx;
	x86.GPRegs[X86::EBX] = regs.ebx;
	x86.GPRegs[X86::ESP] = regs.esp;
	x86.GPRegs[X86::EBP] = regs.ebp;
	x86.GPRegs[X86::ESI] = regs.esi;
	x86.GPRegs[X86::EDI] = regs.edi;
	x86.orig_ax = regs.orig_eax;
	x86.flags = regs.eflags;
	x86.IP = regs.eip;
	x86.segRegs[X86::ES] = regs.xes;
	x86.segRegs[X86::CS] = regs.xcs;
	x86.segRegs[X86::SS] = regs.xss;
	x86.segRegs[X86::DS] = regs.xds;
	x86.segRegs[X86::FS] = regs.xfs;
	x86.segRegs[X86::GS] = regs.xgs;
	x86.filled=true;
}

std::size_t PlatformState::X87::stackPointer() const {
	return (statusWord&0x3800)>>11;
}

std::size_t PlatformState::X87::stIndexToRIndex(std::size_t n) const {

	n=(n+8-stackPointer()) % 8;
	return n;
}

int PlatformState::X87::recreateTag(edb::value80 value) const {
	switch(value.floatType())
	{
	case edb::value80::FloatType::Zero:
		return TAG_ZERO;
	case edb::value80::FloatType::Normal:
		return TAG_VALID;
	default:
		return TAG_SPECIAL;
	}
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
	for(std::size_t n=0;n<FPU_REG_COUNT;++n)
		tagWord |= makeTag(n,twd)<<(2*n);
	return edb::value16(tagWord);
}

void PlatformState::fillFrom(const UserFPRegsStructX86& regs) {
	x87.statusWord=regs.swd; // should be first for stIndexToRIndex() to work
	for(std::size_t n=0;n<FPU_REG_COUNT;++n)
		x87.R[n]=edb::value80(regs.st_space,10*x87.stIndexToRIndex(n));
	x87.controlWord=regs.cwd;
	x87.tagWord=regs.twd; // This is the true tag word, unlike in FPX regs and x86-64 FP regs structs
	x87.instPtrOffset=regs.fip;
	x87.dataPtrOffset=regs.foo;
	x87.instPtrSelector=regs.fcs;
	x87.dataPtrSelector=regs.fos;
	x87.opCode=0; // not present in the given structure
	x87.filled=true;
}
void PlatformState::fillFrom(const UserFPXRegsStructX86& regs) {
	x87.statusWord=regs.swd; // should be first for stIndexToRIndex() to work
	for(std::size_t n=0;n<FPU_REG_COUNT;++n)
		x87.R[n]=edb::value80(regs.st_space,16*x87.stIndexToRIndex(n));
	x87.controlWord=regs.cwd;
	x87.tagWord=x87.restoreTagWord(regs.twd);
	x87.instPtrOffset=regs.fip;
	x87.dataPtrOffset=regs.foo;
	x87.instPtrSelector=regs.fcs;
	x87.dataPtrSelector=regs.fos;
	x87.opCode=regs.fop;
	x87.filled=true;
	x87.opCodeFilled=true;
	for(std::size_t n=0;n<XMM_REG_COUNT;++n)
		avx.setXMM(n,edb::value128(regs.xmm_space,16*n));
	avx.mxcsr=regs.mxcsr;
	avx.xmmFilled=true;
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
	x86.filled=true;
	x86.fsBase = regs.fs_base;
	x86.gsBase = regs.gs_base;
	x86.segBasesFilled=true;
}
void PlatformState::fillFrom(const UserFPRegsStructX86_64& regs) {
	x87.statusWord=regs.swd; // should be first for stIndexToRIndex() to work
	for(std::size_t n=0;n<FPU_REG_COUNT;++n)
		x87.R[n]=edb::value80(regs.st_space,16*x87.stIndexToRIndex(n));
	x87.controlWord=regs.cwd;
	x87.tagWord=x87.restoreTagWord(regs.ftw);
	x87.instPtrOffset=regs.rip; // FIXME
	x87.dataPtrOffset=regs.rdp; // FIXME
	x87.instPtrSelector=0; // FIXME
	x87.dataPtrSelector=0; // FIXME
	x87.opCode=regs.fop;
	x87.filled=true;
	x87.opCodeFilled=true;
	for(std::size_t n=0;n<XMM_REG_COUNT;++n)
		avx.setXMM(n,edb::value128(regs.xmm_space,16*n));
	avx.mxcsr=regs.mxcsr;
	avx.mxcsrMask=regs.mxcr_mask;
	avx.mxcsrMaskFilled=true;
	avx.xmmFilled=true;
}

void PlatformState::fillStruct(UserRegsStructX86& regs) const
{
	util::markMemory(&regs,sizeof(regs));
	if(x86.filled) {
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
	// Put some markers to make invalid values immediately visible
	util::markMemory(&regs,sizeof(regs));
	if(x86.filled) {
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
		regs.fs_base=x86.fsBase;
		regs.gs_base=x86.gsBase;
		regs.orig_rax=x86.orig_ax;
		regs.eflags=x86.flags;
		regs.rip=x86.IP;
	}
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

void PlatformState::AVX::setYMM(std::size_t index, edb::value256 value) {
	// leave upper part unchanged.
	std::memcpy(&zmmStorage[index],&value,sizeof value);
}

void PlatformState::AVX::setZMM(std::size_t index, edb::value512 value) {
	zmmStorage[index]=value;
}

void PlatformState::X86::clear() {
	util::markMemory(this,sizeof(*this));
	filled=false;
	segBasesFilled=false;
}

void PlatformState::X87::clear() {
	util::markMemory(this,sizeof(*this));
	filled=false;
	opCodeFilled=false;
}

void PlatformState::AVX::clear() {
	util::markMemory(this,sizeof(*this));
	xmmFilled=false;
	ymmFilled=false;
	zmmFilled=false;
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
	char buf[14];
	qsnprintf(
		buf,
		sizeof(buf),
		"%c %c %c %c %c %c %c",
		((flags & 0x001) ? 'C' : 'c'),
		((flags & 0x004) ? 'P' : 'p'),
		((flags & 0x010) ? 'A' : 'a'),
		((flags & 0x040) ? 'Z' : 'z'),
		((flags & 0x080) ? 'S' : 's'),
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

template<typename Names, typename Regs>
Register findRegisterValue(const Names& names, const Regs& regs, const QString& regName, Register::Type type, edb::reg_t mask=~0UL, int shift=0)
{
	auto regNameFoundIter=std::find(names.begin(),names.end(),regName);
	if(regNameFoundIter!=names.end())
		return Register(regName, (regs[regNameFoundIter-names.begin()]>>shift) & mask, type);
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
	if(x86.filled) // don't return valid Register with garbage value
	{
		if(static_cast<void*>(found=findRegisterValue(x86.GPRegNames, x86.GPRegs, regName, Register::TYPE_GPR)))
			return found;
		// On IA-32 this is duplicate of the above; hopefully the compiler will optimize this out. Not a big deal if not.
		if(static_cast<void*>(found=findRegisterValue(x86.GPReg32Names, x86.GPRegs, regName, Register::TYPE_GPR,0xffffffff)))
			return found;
		if(static_cast<void*>(found=findRegisterValue(x86.GPReg16Names, x86.GPRegs, regName, Register::TYPE_GPR,0xffff)))
			return found;
		if(static_cast<void*>(found=findRegisterValue(x86.GPReg8LNames, x86.GPRegs, regName, Register::TYPE_GPR,0xff)))
			return found;
		if(static_cast<void*>(found=findRegisterValue(x86.GPReg8HNames, x86.GPRegs, regName, Register::TYPE_GPR,0xff, 8)))
			return found;
		if(static_cast<void*>(found=findRegisterValue(x86.segRegNames, x86.segRegs, regName, Register::TYPE_SEG)))
			return found;
		if(regName==x86.fsBaseName)
			return Register(x86.fsBaseName, x86.fsBase, Register::TYPE_SEG); // FIXME: it's not a segment register, it's an address
		if(regName==x86.gsBaseName)
			return Register(x86.gsBaseName, x86.gsBase, Register::TYPE_SEG); // FIXME: it's not a segment register, it's an address
		if(regName==x86.flagsName)
			return Register(x86.flagsName, x86.flags, Register::TYPE_COND);
		if(regName==x86.IPName)
			return Register(x86.IPName, x86.IP, Register::TYPE_IP);
	}
	if(avx.xmmFilled && regName==avx.mxcsrName)
		return Register(avx.mxcsrName, edb::reg_t::fromZeroExtended(avx.mxcsr), Register::TYPE_COND);
	return Register();
}

//------------------------------------------------------------------------------
// Name: frame_pointer
// Desc: returns what is conceptually the frame pointer for this platform
//------------------------------------------------------------------------------
edb::address_t PlatformState::frame_pointer() const {
	return x86.GPRegs[X86::RSP];
}

//------------------------------------------------------------------------------
// Name: instruction_pointer
// Desc: returns the instruction pointer for this platform
//------------------------------------------------------------------------------
edb::address_t PlatformState::instruction_pointer() const {
	return x86.IP;
}

//------------------------------------------------------------------------------
// Name: stack_pointer
// Desc: returns the stack pointer for this platform
//------------------------------------------------------------------------------
edb::address_t PlatformState::stack_pointer() const {
	return x86.GPRegs[X86::RSP];
}

//------------------------------------------------------------------------------
// Name: debug_register
// Desc:
//------------------------------------------------------------------------------
edb::reg_t PlatformState::debug_register(int n) const {
	assert(dbgIndexValid(n));
	return x86.dbgRegs[n];
}

//------------------------------------------------------------------------------
// Name: flags
// Desc:
//------------------------------------------------------------------------------
edb::reg_t PlatformState::flags() const {
	return x86.flags;
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
edb::value80 PlatformState::fpu_register(int n) const {
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
// Name: set_debug_register
// Desc:
//------------------------------------------------------------------------------
void PlatformState::set_debug_register(int n, edb::reg_t value) {
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
// Name: set_register
// Desc:
//------------------------------------------------------------------------------
Register PlatformState::gp_register(int n) const {

	if(gprIndexValid(n))
		return Register(x86.GPRegNames[n], x86.GPRegs[n], Register::TYPE_GPR);
	else if(n==GPR_COUNT) // This value is used as an alias for program counter, although it's not a GPR
		return Register(x86.IPName, x86.IP, Register::TYPE_IP);
	else
		return Register();
}

//------------------------------------------------------------------------------
// Name: set_register
// Desc:
//------------------------------------------------------------------------------
void PlatformState::set_register(const QString &name, edb::reg_t value) {

	const QString regName = name.toLower();
	auto GPRegNameFoundIter=std::find(x86.GPRegNames.begin(), x86.GPRegNames.end(), regName);
	if(GPRegNameFoundIter!=x86.GPRegNames.end())
	{
		std::size_t index=GPRegNameFoundIter-x86.GPRegNames.begin();
		x86.GPRegs[index]=value;
		return;
	}
	auto segRegNameFoundIter=std::find(x86.segRegNames.begin(), x86.segRegNames.end(), regName);
	if(segRegNameFoundIter!=x86.segRegNames.end())
	{
		std::size_t index=segRegNameFoundIter-x86.segRegNames.begin();
		x86.segRegs[index]=edb::seg_reg_t(value);
		return;
	}
	if(regName==x86.flagsName)
	{
		x86.flags=value;
		return;
	}
	if(regName==avx.mxcsrName)
	{
		avx.mxcsr=edb::value32(value);
		return;
	}
}

//------------------------------------------------------------------------------
// Name: mmx_register
// Desc:
//------------------------------------------------------------------------------
edb::value64 PlatformState::mmx_register(int n) const {
	assert(mmxIndexValid(n));
	return x87.R[n].mantissa();
}

//------------------------------------------------------------------------------
// Name: xmm_register
// Desc:
//------------------------------------------------------------------------------
edb::value128 PlatformState::xmm_register(int n) const {
    assert(xmmIndexValid(n));
	return avx.xmm(n);
}

}
