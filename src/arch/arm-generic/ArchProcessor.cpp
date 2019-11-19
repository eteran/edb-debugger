/*
Copyright (C) 2006 - 2015 Evan Teran
                          evan.teran@gmail.com
Copyright (C) 2017 - 2017 Ruslan Kabatsayev
                          b7.10110111@gmail.com

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

#include "ArchProcessor.h"
#include "Configuration.h"
#include "IDebugger.h"
#include "RegisterViewModel.h"
#include "State.h"
#include "Util.h"
#include "edb.h"

#include <QWidget>
#include <cstdint>

using std::uint32_t;

namespace {
static constexpr size_t GPR_COUNT = 16;
}

int capstoneRegToGPRIndex(int capstoneReg, bool &ok) {

	ok           = false;
	int regIndex = -1;
	// NOTE: capstone registers are stupidly not in continuous order
	switch (capstoneReg) {
	case ARM_REG_R0:
		regIndex = 0;
		break;
	case ARM_REG_R1:
		regIndex = 1;
		break;
	case ARM_REG_R2:
		regIndex = 2;
		break;
	case ARM_REG_R3:
		regIndex = 3;
		break;
	case ARM_REG_R4:
		regIndex = 4;
		break;
	case ARM_REG_R5:
		regIndex = 5;
		break;
	case ARM_REG_R6:
		regIndex = 6;
		break;
	case ARM_REG_R7:
		regIndex = 7;
		break;
	case ARM_REG_R8:
		regIndex = 8;
		break;
	case ARM_REG_R9:
		regIndex = 9;
		break;
	case ARM_REG_R10:
		regIndex = 10;
		break;
	case ARM_REG_R11:
		regIndex = 11;
		break;
	case ARM_REG_R12:
		regIndex = 12;
		break;
	case ARM_REG_R13:
		regIndex = 13;
		break;
	case ARM_REG_R14:
		regIndex = 14;
		break;
	case ARM_REG_R15:
		regIndex = 15;
		break;
	default:
		return -1;
	}
	ok = true;
	return regIndex;
}

Result<edb::address_t, QString> getOperandValueGPR(const edb::Instruction &insn, const edb::Operand &operand, const State &state) {

	bool ok;
	const auto regIndex = capstoneRegToGPRIndex(operand->reg, ok);
	if (!ok) {
		return make_unexpected(QObject::tr("bad operand register for instruction %1: %2.").arg(insn.mnemonic().c_str()).arg(operand->reg));
	}

	const auto reg = state.gpRegister(regIndex);
	if (!reg) {
		return make_unexpected(QObject::tr("failed to get register r%1.").arg(regIndex));
	}

	return reg.valueAsAddress();
}

Result<edb::address_t, QString> adjustR15Value(const edb::Instruction &insn, const int regIndex, edb::address_t value) {

	if (regIndex == 15) {
		// Even if current state's PC weren't on this instruction, the instruction still refers to
		// self, so use `insn` instead of `state` to get the value.
		const auto cpuMode = edb::v1::debugger_core->cpuMode();
		switch (cpuMode) {
		case IDebugger::CpuMode::ARM32:
			value = insn.rva() + 8;
			break;
		case IDebugger::CpuMode::Thumb:
			value = insn.rva() + 4;
			break;
		default:
			return make_unexpected(QObject::tr("calculating effective address in modes other than ARM and Thumb is not supported."));
		}
	}

	return value;
}

uint32_t shift(uint32_t x, arm_shifter type, uint32_t shiftAmount, bool carryFlag) {
	constexpr uint32_t HighBit = 1u << 31;
	const uint32_t N           = shiftAmount;

	switch (type) {
	case ARM_SFT_INVALID:
		return x;
	case ARM_SFT_ASR:
	case ARM_SFT_ASR_REG:
		assert(N >= 1 && N <= 32);
		// NOTE: unlike on x86, shift by 32 bits on ARM is not a NOP: it sets all bits to sign bit
		return N == 32 ? -((x & HighBit) >> 31) : x >> N | ~(((x & HighBit) >> N) - 1);
	case ARM_SFT_LSL:
	case ARM_SFT_LSL_REG:
		assert(N >= 0 && N <= 31);
		return x << N;
	case ARM_SFT_LSR:
	case ARM_SFT_LSR_REG:
		// NOTE: unlike on x86, shift by 32 bits on ARM is not a NOP: it clears the value
		return N == 32 ? 0 : x >> N;
	case ARM_SFT_ROR:
	case ARM_SFT_ROR_REG: {
		assert(N >= 1 && N <= 31);
		constexpr unsigned mask = 8 * sizeof x - 1;
		return x >> N | x << ((-N) & mask);
	}
	case ARM_SFT_RRX:
	case ARM_SFT_RRX_REG:
		return uint32_t(carryFlag) << 31 | x >> 1;
	}
	assert(!"Must not reach here!");
	return x;
}

// NOTE: this function shouldn't be used for operands other than those used as addresses.
// E.g. for "STM Rn,{regs...}" this function shouldn't try to get the value of any of the {regs...}.
// Also note that undefined instructions like "STM PC, {regs...}" aren't checked here.
Result<edb::address_t, QString> ArchProcessor::getEffectiveAddress(const edb::Instruction &insn, const edb::Operand &operand, const State &state) const {

	if (!operand || !insn) {
		return make_unexpected(QObject::tr("operand is invalid"));
	}

	const auto op = insn.operation();
	if (is_register(operand)) {
		bool ok;
		const auto regIndex = capstoneRegToGPRIndex(operand->reg, ok);
		if (!ok) return make_unexpected(QObject::tr("bad operand register for instruction %1: %2.").arg(insn.mnemonic().c_str()).arg(operand->reg));
		const auto reg = state.gpRegister(regIndex);
		if (!reg) return make_unexpected(QObject::tr("failed to get register r%1.").arg(regIndex));
		auto value = reg.valueAsAddress();
		return adjustR15Value(insn, regIndex, value);
	} else if (is_expression(operand)) {
		bool ok;
		Register baseR, indexR, cpsrR;

		const auto baseIndex = capstoneRegToGPRIndex(operand->mem.base, ok);
		// base must be valid
		if (!ok || !(baseR = state.gpRegister(baseIndex)))
			return make_unexpected(QObject::tr("failed to get register r%1.").arg(baseIndex));

		const auto indexIndex = capstoneRegToGPRIndex(operand->mem.index, ok);
		if (ok) // index register may be irrelevant, only try to get it if its index is valid
		{
			if (!(indexR = state.gpRegister(indexIndex)))
				return make_unexpected(QObject::tr("failed to get register r%1.").arg(indexIndex));
		}

		cpsrR = state.flagsRegister();
		if (!cpsrR && (operand->shift.type == ARM_SFT_RRX || operand->shift.type == ARM_SFT_RRX_REG))
			return make_unexpected(QObject::tr("failed to get CPSR."));
		const bool C = cpsrR ? cpsrR.valueAsInteger() & 0x20000000 : false;

		edb::address_t addr = baseR.valueAsAddress();
		// TODO(eteran): why does making this const cause an error on the return? Bug in conversion constructor for Result?
		if (auto adjustedRes = adjustR15Value(insn, baseIndex, addr)) {
			addr = adjustedRes.value() + operand->mem.disp;
			if (indexR) {
				addr += operand->mem.scale * shift(indexR.valueAsAddress(), operand->shift.type, operand->shift.value, C);
			}

			return addr;
		} else
			return adjustedRes;
	}

	return make_unexpected(QObject::tr("getting effective address for operand %1 of instruction %2 is not implemented").arg(operand.index() + 1).arg(insn.mnemonic().c_str()));
}

edb::address_t ArchProcessor::getEffectiveAddress(const edb::Instruction &inst, const edb::Operand &op, const State &state, bool &ok) const {

	ok                = false;
	const auto result = getEffectiveAddress(inst, op, state);
	if (!result) return 0;
	return result.value();
}

RegisterViewModel &getModel() {
	return static_cast<RegisterViewModel &>(edb::v1::arch_processor().registerViewModel());
}

ArchProcessor::ArchProcessor() {
	if (edb::v1::debugger_core) {
		connect(edb::v1::debugger_ui, SIGNAL(attachEvent()), this, SLOT(justAttached()));
	}
}

QStringList ArchProcessor::updateInstructionInfo(edb::address_t address) {
	Q_UNUSED(address)
	QStringList ret;
	return ret;
}

bool ArchProcessor::canStepOver(const edb::Instruction &inst) const {
	return inst && (is_call(inst) || is_interrupt(inst) || !modifies_pc(inst));
}

bool ArchProcessor::isFilling(const edb::Instruction &inst) const {
	Q_UNUSED(inst)
	return false;
}

void ArchProcessor::reset() {
}

void ArchProcessor::aboutToResume() {
	getModel().saveValues();
}

void ArchProcessor::setupRegisterView() {

	if (edb::v1::debugger_core) {
		updateRegisterView(QString(), State());
	}
}

QString pcComment(Register const &reg, QString const &default_region_name) {
	const auto symname = edb::v1::find_function_symbol(reg.valueAsAddress(), default_region_name);
	return symname.isEmpty() ? symname : '<' + symname + '>';
}

QString gprComment(Register const &reg) {

	QString regString;
	int stringLength;
	QString comment;
	if (edb::v1::get_ascii_string_at_address(reg.valueAsAddress(), regString, edb::v1::config().min_string_length, 256, stringLength))
		comment = QString("ASCII \"%1\"").arg(regString);
	else if (edb::v1::get_utf16_string_at_address(reg.valueAsAddress(), regString, edb::v1::config().min_string_length, 256, stringLength))
		comment = QString("UTF16 \"%1\"").arg(regString);
	return comment;
}

void updateGPRs(RegisterViewModel &model, State const &state, QString const &default_region_name) {
	for (std::size_t i = 0; i < GPR_COUNT; ++i) {
		const auto reg = state.gpRegister(i);
		Q_ASSERT(!!reg);
		Q_ASSERT(reg.bitSize() == 32);
		QString comment;
		if (i != 15)
			comment = gprComment(reg);
		else
			comment = pcComment(reg, default_region_name);
		model.updateGPR(i, reg.value<edb::value32>(), comment);
	}
}

bool is_jcc_taken(const edb::reg_t cpsr, edb::Instruction::ConditionCode cond) {
	const bool N = (cpsr & 0x80000000) != 0;
	const bool Z = (cpsr & 0x40000000) != 0;
	const bool C = (cpsr & 0x20000000) != 0;
	const bool V = (cpsr & 0x10000000) != 0;

	bool taken = false;
	switch (cond & 0xe) {
	case 0x0:
		taken = Z;
		break;
	case 0x2:
		taken = C;
		break;
	case 0x4:
		taken = N;
		break;
	case 0x6:
		taken = V;
		break;
	case 0x8:
		taken = C && !Z;
		break;
	case 0xa:
		taken = N == V;
		break;
	case 0xc:
		taken = !Z && (N == V);
		break;
	case 0xe:
		taken = true;
		break;
	}

	if (cond & 1)
		taken = !taken;

	return taken;
}

static const QLatin1String jumpConditionMnemonics[] = {
	QLatin1String("EQ"),
	QLatin1String("NE"),
	QLatin1String("HS"),
	QLatin1String("LO"),
	QLatin1String("MI"),
	QLatin1String("PL"),
	QLatin1String("VS"),
	QLatin1String("VC"),
	QLatin1String("HI"),
	QLatin1String("LS"),
	QLatin1String("GE"),
	QLatin1String("LT"),
	QLatin1String("GT"),
	QLatin1String("LE"),
	QLatin1String("AL"),
	QLatin1String("??"),
};

QString cpsrComment(edb::reg_t flags) {
	QString comment = "(";
	for (int cond = 0; cond < 0x10 - 2; ++cond) // we're not interested in AL or UNDEFINED conditions
		if (is_jcc_taken(flags, static_cast<edb::Instruction::ConditionCode>(cond)))
			comment += jumpConditionMnemonics[cond] + ',';
	comment[comment.size() - 1] = ')';
	return comment;
}

void updateCPSR(RegisterViewModel &model, State const &state) {
	const auto flags = state.flagsRegister();
	Q_ASSERT(!!flags);
	const auto comment = cpsrComment(flags.valueAsInteger());
	model.updateCPSR(flags.value<edb::value32>(), comment);
}

QString fpscrComment(edb::reg_t fpscr) {
	const auto nzcv = fpscr >> 28;
	switch (nzcv) {
	case 2:
		return "(GT)";
	case 3:
		return "(Unordered)";
	case 6:
		return "(EQ)";
	case 8:
		return "(LT)";
	default:
		return "";
	}
}

void updateVFP(RegisterViewModel &model, State const &state) {
	const auto fpscr = state["fpscr"];
	if (fpscr) {
		const auto comment = fpscrComment(fpscr.valueAsInteger());
		model.updateFPSCR(fpscr.value<edb::value32>(), comment);
	}
}

void ArchProcessor::updateRegisterView(const QString &default_region_name, const State &state) {

	auto &model = getModel();
	if (state.empty()) {
		model.setCpuMode(RegisterViewModel::CpuMode::UNKNOWN);
		return;
	}

	model.setCpuMode(RegisterViewModel::CpuMode::Defined);

	updateGPRs(model, state, default_region_name);
	updateCPSR(model, state);
	updateVFP(model, state);

	if (justAttached_) {
		model.saveValues();
		justAttached_ = false;
	}
	model.dataUpdateFinished();
}

RegisterViewModelBase::Model &ArchProcessor::registerViewModel() const {
	static RegisterViewModel model(0);
	return model;
}

void ArchProcessor::justAttached() {
	justAttached_ = true;
}

bool ArchProcessor::isExecuted(const edb::Instruction &inst, const State &state) const {
	return is_jcc_taken(state.flags(), inst.conditionCode());
}
