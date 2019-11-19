/*
Copyright (C) 2017 Ruslan Kabatsayev
                   b7.1010111@gmail.com

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

#ifndef PLATFORM_STATE_H_20170806_
#define PLATFORM_STATE_H_20170806_

#include "IState.h"
#include "PrStatus.h"
#include "Types.h"
#include "edb.h"
#include <cstddef>
#include <sys/user.h>
#include <vector>

namespace DebuggerCorePlugin {

using std::size_t;
static constexpr size_t GPR_COUNT  = 16;
static constexpr size_t VFPR_COUNT = 32;

struct user_vfp {
	unsigned long long fpregs[32];
	unsigned long fpscr;
};

class PlatformState final : public IState {
	Q_DECLARE_TR_FUNCTIONS(PlatformThread)
	friend class DebuggerCore;
	friend class PlatformThread;

public:
	PlatformState();

public:
	std::unique_ptr<IState> clone() const override;

	QString flagsToString() const override;
	QString flagsToString(edb::reg_t flags) const override;
	Register value(const QString &reg) const override;
	Register instructionPointerRegister() const override;
	Register flagsRegister() const override;
	edb::address_t framePointer() const override;
	edb::address_t instructionPointer() const override;
	edb::address_t stackPointer() const override;
	edb::reg_t debugRegister(size_t n) const override;
	edb::reg_t flags() const override;
	void adjustStack(int bytes) override;
	void clear() override;
	bool empty() const override;
	void setDebugRegister(size_t n, edb::reg_t value) override;
	void setFlags(edb::reg_t flags) override;
	void setInstructionPointer(edb::address_t value) override;
	void setRegister(const Register &reg) override;
	void setRegister(const QString &name, edb::reg_t value) override;
	Register gpRegister(size_t n) const override;

	Register archRegister(uint64_t type, size_t n) const override {
		return Register();
	}

	void fillFrom(const user_regs &regs);
	void fillFrom(const user_vfp &regs);
	void fillStruct(user_regs &regs) const;
	void fillStruct(user_vfp &regs) const;

private:
	struct GPR {
		enum NamedGPRIndex : size_t {
			SB = 9,  // historical name, but still printed by modern disassemblers
			SL = 10, // historical name, but still printed by modern disassemblers
			FP = 11, // conventionally, but much like rBP on x86
			IP = 12, // conventionally, intra-procedure scratch register
			SP = 13,
			LR = 14,
			PC = 15,
		};
		using RegNameVariants = std::vector<const char *>;
		static const std::array<RegNameVariants, GPR_COUNT> GPRegNames;

		bool filled = false;
		std::array<edb::reg_t, GPR_COUNT> GPRegs;
		edb::reg_t cpsr;

	public:
		void clear();
		bool empty() const;
	} gpr;
	struct VFP {
		std::array<edb::value64, VFPR_COUNT> d;
		edb::value32 fpscr;
		bool filled = false;
	} vfp;

private:
	auto findGPR(const QString &name) const -> decltype(gpr.GPRegNames.begin());
};

}

#endif
