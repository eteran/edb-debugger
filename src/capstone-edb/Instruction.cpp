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

#include "Instruction.h"

#include <QRegExp>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace CapstoneEDB {

namespace {

constexpr int MaxOperands = 3;

Architecture capstoneArch = Architecture::ARCH_X86;
bool capstoneInitialized  = false;
csh csh                   = 0;
Formatter activeFormatter;

#if defined(EDB_X86) || defined(EDB_X86_64)
/**
 * @brief is_simd_register
 * @param operand
 * @return
 */
bool is_simd_register(const Operand &operand) {

	if (operand->type != X86_OP_REG)
		return false;

	const auto reg = operand->reg;

	if (X86_REG_MM0 <= reg && reg <= X86_REG_MM7)
		return true;
	if (X86_REG_XMM0 <= reg && reg <= X86_REG_XMM31)
		return true;
	if (X86_REG_YMM0 <= reg && reg <= X86_REG_YMM31)
		return true;
	if (X86_REG_ZMM0 <= reg && reg <= X86_REG_ZMM31)
		return true;

	return false;
}

/**
 * @brief apriori_not_simd
 * @param insn
 * @param operand
 * @return
 */
bool apriori_not_simd(const Instruction &insn, const Operand &operand) {

	if (!is_simd(insn))
		return true;

	if (operand->type == X86_OP_REG && !is_simd_register(operand))
		return true;
	if (operand->type == X86_OP_IMM)
		return true;

	return false;
}

/**
 * @brief KxRegisterPresent
 * @param insn
 * @return
 */
bool KxRegisterPresent(const Instruction &insn) {
	const size_t operandCount = insn.operandCount();

	for (std::size_t i = 0; i < operandCount; ++i) {
		const auto op = insn[i];
		if (op->type == X86_OP_REG && X86_REG_K0 <= op->reg && op->reg <= X86_REG_K7) {
			return true;
		}
	}
	return false;
}

/**
 * @brief simdOperandNormalizedNumberInInstruction
 * @param insn
 * @param operand
 * @param canBeNonSIMD
 * @return
 */
std::size_t simdOperandNormalizedNumberInInstruction(const Instruction &insn, const Operand &operand, bool canBeNonSIMD = false) {

	if (!canBeNonSIMD)
		assert(!apriori_not_simd(insn, operand));

	size_t number             = operand.index();
	const size_t operandCount = insn.operandCount();

	// normalized number is according to Intel order
	if (activeFormatter.options().syntax == Formatter::SyntaxAtt) {
		assert(number < operandCount);
		number = operandCount - 1 - number;
	}

	if (number > 0 && KxRegisterPresent(insn)) {
		--number;
	}

	return number;
}
#endif

/**
 * @brief isX86_64
 * @return
 */
bool isX86_64() {
	return capstoneArch == Architecture::ARCH_AMD64;
}

/**
 * @brief to_operands
 * @param str
 * @return
 */
std::vector<std::string> to_operands(QString str) {

	// Remove any decorations: we want just operands themselves
	static const QRegularExpression re(",?\\{[^}]*\\}");
	str.replace(re, "");

	QStringList betweenCommas = str.split(",");
	std::vector<std::string> operands;

	// Have to work around inconvenient AT&T syntax for SIB, that's why so complicated logic
	for (auto it = betweenCommas.begin(); it != betweenCommas.end(); ++it) {

		QString &current(*it);

		// We've split operand string by commas, but there may be SIB scheme
		// in the form (B,I,S) or (B) or (I,S). Let's find missing parts of it.
		if (it->contains("(") && !it->contains(")")) {

			std::logic_error matchFailed("failed to find matching ')'");

			// the next part must exist and have continuation of SIB scheme
			if (std::next(it) == betweenCommas.end()) {
				throw matchFailed;
			}

			current += ",";
			current += *(++it);

			// This may still be not enough
			if (current.contains("(") && !current.contains(")")) {
				if (std::next(it) == betweenCommas.end()) {
					throw matchFailed;
				}

				current += ",";
				current += *(++it);
			}

			// The expected SIB string has at most three components.
			// If we still haven't found closing parenthesis, we're screwed
			if (current.contains("(") && !current.contains(")")) {
				throw matchFailed;
			}
		}
		operands.push_back(current.trimmed().toStdString());
	}

	if (operands.size() > MaxOperands) {
		throw std::logic_error("got more than " + std::to_string(MaxOperands) + " operands");
	}

	return operands;
}

}

bool init(Architecture arch) {

	capstoneArch = arch;

	if (capstoneInitialized) {
		cs_close(&csh);
	}

	capstoneInitialized = false;

	const cs_err result = [arch]() {
		switch (arch) {
		case Architecture::ARCH_AMD64:
			return cs_open(CS_ARCH_X86, CS_MODE_64, &csh);
		case Architecture::ARCH_X86:
			return cs_open(CS_ARCH_X86, CS_MODE_32, &csh);
		case Architecture::ARCH_ARM32_ARM:
			return cs_open(CS_ARCH_ARM, CS_MODE_ARM, &csh);
		case Architecture::ARCH_ARM32_THUMB:
			return cs_open(CS_ARCH_ARM, CS_MODE_THUMB, &csh);
		case Architecture::ARCH_ARM64:
			return cs_open(CS_ARCH_ARM64, CS_MODE_ARM, &csh);
		default:
			return CS_ERR_ARCH;
		}
	}();

	if (result != CS_ERR_OK) {
		return false;
	}

	capstoneInitialized = true;

	cs_option(csh, CS_OPT_DETAIL, CS_OPT_ON);

	// Set selected formatting options on reinit
	activeFormatter.setOptions(activeFormatter.options());
	return true;
}

Instruction::Instruction(Instruction &&other) noexcept
	: insn_(other.insn_), byte0_(other.byte0_), rva_(other.rva_) {

	other.insn_  = nullptr;
	other.byte0_ = 0;
	other.rva_   = 0;
}

Instruction &Instruction::operator=(Instruction &&rhs) noexcept {
	insn_      = rhs.insn_;
	byte0_     = rhs.byte0_;
	rva_       = rhs.rva_;
	rhs.insn_  = nullptr;
	rhs.byte0_ = 0;
	rhs.rva_   = 0;
	return *this;
}

Instruction::~Instruction() {
	if (insn_) {
		cs_free(insn_, 1);
	}
}

Instruction::Instruction(const void *first, const void *last, uint64_t rva) noexcept
	: rva_(rva) {

	assert(capstoneInitialized);
	auto codeBegin = static_cast<const uint8_t *>(first);
	auto codeEnd   = static_cast<const uint8_t *>(last);

	byte0_ = codeBegin[0];

	cs_insn *insn = nullptr;
	if (first < last && cs_disasm(csh, codeBegin, codeEnd - codeBegin, rva, 1, &insn)) {
		insn_ = insn;
#if defined(EDB_ARM32)
		if (insn_->detail->arm.op_count >= 2) {
			// XXX: this is a work around capstone bug #1013
			auto &op = insn_->detail->arm.operands[1];
			if (op.type == ARM_OP_MEM && op.subtracted && op.mem.scale == 1)
				op.mem.scale = -1;
		}
#endif
	} else {
		insn_ = nullptr;
	}
}

Operand Instruction::operator[](size_t n) const {
	if (!valid())
		return Operand();
	if (n > operandCount())
		return Operand();

#if defined(EDB_X86) || defined(EDB_X86_64)
	return Operand(this, &insn_->detail->x86.operands[n], n);
#elif defined(EDB_ARM32) || defined(EDB_ARM64)
	return Operand(this, &insn_->detail->arm.operands[n], n);
#else
#error "What to return here?"
#endif
}

Operand Instruction::operand(size_t n) const {
	if (!valid())
		return Operand();
	if (n > operandCount())
		return Operand();

#if defined(EDB_X86) || defined(EDB_X86_64)
	return Operand(this, &insn_->detail->x86.operands[n], n);
#elif defined(EDB_ARM32) || defined(EDB_ARM64)
	return Operand(this, &insn_->detail->arm.operands[n], n);
#else
#error "What to return here?"
#endif
}

Instruction::ConditionCode Instruction::conditionCode() const {

#if defined(EDB_X86) || defined(EDB_X86_64)
	switch (operation()) {
	// J*CXZ
	case X86_INS_JRCXZ:
		return CC_RCXZ;
	case X86_INS_JECXZ:
		return CC_ECXZ;
	case X86_INS_JCXZ:
		return CC_CXZ;
	// FPU conditional move
	case X86_INS_FCMOVBE:
		return CC_BE;
	case X86_INS_FCMOVB:
		return CC_B;
	case X86_INS_FCMOVE:
		return CC_E;
	case X86_INS_FCMOVNBE:
		return CC_NBE;
	case X86_INS_FCMOVNB:
		return CC_NB;
	case X86_INS_FCMOVNE:
		return CC_NE;
	case X86_INS_FCMOVNU:
		return CC_NP;
	case X86_INS_FCMOVU:
		return CC_P;
	// TODO: handle LOOPcc, REPcc OP
	default:
		if (is_conditional_gpr_move(*this) || is_conditional_jump(*this) || is_conditional_set(*this)) {
			const uint8_t *opcode = insn_->detail->x86.opcode;
			if (opcode[0] == 0x0f)
				return static_cast<ConditionCode>(opcode[1] & 0xf);
			return static_cast<ConditionCode>(opcode[0] & 0xf);
		}
	}
	return CC_UNCONDITIONAL;
#elif defined(EDB_ARM32)
	switch (insn_->detail->arm.cc) {
	case ARM_CC_EQ:
		return CC_EQ;
	case ARM_CC_NE:
		return CC_NE;
	case ARM_CC_HS:
		return CC_HS;
	case ARM_CC_LO:
		return CC_LO;
	case ARM_CC_MI:
		return CC_MI;
	case ARM_CC_PL:
		return CC_PL;
	case ARM_CC_VS:
		return CC_VS;
	case ARM_CC_VC:
		return CC_VC;
	case ARM_CC_HI:
		return CC_HI;
	case ARM_CC_LS:
		return CC_LS;
	case ARM_CC_GE:
		return CC_GE;
	case ARM_CC_LT:
		return CC_LT;
	case ARM_CC_GT:
		return CC_GT;
	case ARM_CC_LE:
		return CC_LE;
	case ARM_CC_AL:
		return CC_AL;
	default:
		return CC_AL;
	}
#else
#error "Not implemented"
#endif
}

void Instruction::swap(Instruction &other) {
	using std::swap;
	swap(insn_, other.insn_);
	swap(byte0_, other.byte0_);
	swap(rva_, other.rva_);
}

QString Formatter::adjustInstructionText(const Instruction &insn) const {

	QString operands(insn->op_str);

	// Remove extra spaces
	operands.replace(" + ", "+");
	operands.replace(" - ", "-");

	operands.replace(QRegExp("\\bxword "), "tbyte ");
	operands.replace(QRegExp("(word|byte) ptr "), "\\1 ");

#if defined(EDB_X86) || defined(EDB_X86_64)
	if (activeFormatter.options().simplifyRIPRelativeTargets && isX86_64() && (insn->detail->x86.modrm & 0xc7) == 0x05) {
		QRegExp ripRel("\\brip ?[+-] ?((0x)?[0-9a-fA-F]+)\\b");
		operands.replace(ripRel, "rel 0x" + QString::number(insn->detail->x86.disp + insn->address + insn->size, 16));
	}

	if (insn.operandCount() == 2 && insn->id != X86_INS_MOVZX && insn->id != X86_INS_MOVSX &&
		((insn[0]->type == X86_OP_REG && insn[1]->type == X86_OP_MEM) || (insn[1]->type == X86_OP_REG && insn[0]->type == X86_OP_MEM))) {
		operands.replace(QRegExp("(\\b.?(mm)?word|byte)\\b( ptr)? "), "");
	}
#endif
	return operands;
}

void Formatter::setOptions(const Formatter::FormatOptions &options) {
	assert(capstoneInitialized);

	options_ = options;

#if defined(EDB_X86) || defined(EDB_X86_64)
	if (options.syntax == SyntaxAtt) {
		cs_option(csh, CS_OPT_SYNTAX, CS_OPT_SYNTAX_ATT);
	} else {
		cs_option(csh, CS_OPT_SYNTAX, CS_OPT_SYNTAX_INTEL);
	}
#elif defined(EDB_ARM32) // FIXME(ARM): does this apply to AArch64?
	// TODO: make this optional. Don't forget to reflect this in register view!
	cs_option(csh, CS_OPT_SYNTAX, CS_OPT_SYNTAX_NOREGNAME);
#endif

	activeFormatter = *this;
}

std::string Formatter::toString(const Instruction &insn) const {

	enum {
		Tab1Size = 8,
		Tab2Size = 11,
	};

	if (!insn) {
		char buf[32];
		if (options_.tabBetweenMnemonicAndOperands) {
			snprintf(buf, sizeof(buf), "%-*s0x%02x", Tab1Size, "db", insn.byte0_);
		} else {
			snprintf(buf, sizeof(buf), "db 0x%02x", insn.byte0_);
		}

		std::string str(buf);
		checkCapitalize(str);
		return str;
	}

	std::ostringstream s;
	s << insn->mnemonic;
	std::string space = " ";
	if (options_.tabBetweenMnemonicAndOperands) {
		const auto pos = s.tellp();
		const auto pad = pos < Tab1Size ? Tab1Size - pos : pos < Tab2Size ? Tab2Size - pos : 1;
		space          = std::string(pad, ' ');
	}
	if (insn.operandCount() > 0) // prevent addition of trailing whitespace
	{
		s << space << adjustInstructionText(insn).toStdString();
	} else if (insn->op_str[0] != 0) {
		// This may happen for instructions like IT in Thumb-2: e.g. ITT NE
		s << space << insn->op_str;
	}

	auto str = s.str();
	checkCapitalize(str);
	return str;
}

void Formatter::checkCapitalize(std::string &str, bool canContainHex) const {
	if (options_.capitalization == UpperCase) {
		std::transform(str.begin(), str.end(), str.begin(), ::toupper);
		if (canContainHex) {
			QString qstr = QString::fromStdString(str);

			const QRegularExpression re("\\b0X([0-9A-F]+)\\b");
			qstr.replace(re, "0x\\1");

			str = qstr.toStdString();
		}
	}
}

std::string Formatter::toString(const Operand &operand) const {
	if (!operand)
		return "(bad)";

	std::string str;

	const Instruction &insn = *operand.owner();

	const std::size_t totalOperands       = insn.operandCount();
	const std::size_t numberInInstruction = operand.index();
#if defined(EDB_X86) || defined(EDB_X86_64)
	if (operand->type == X86_OP_REG) {
#elif defined(EDB_ARM32) || defined(EDB_ARM64)
	if (operand->type == ARM_OP_REG) {
#endif
		str = registerName(operand->reg);
	} else if (totalOperands == 1) {
		str = insn->op_str;
	} else {
		// Capstone doesn't provide a way to get operand string, so we try
		// to extract it from the formatted all-operands string
		try {
			const auto operands = to_operands(insn->op_str);

			if (operands.size() <= numberInInstruction) {
				throw std::logic_error("got less than " + std::to_string(numberInInstruction) + " operands");
			}

			str = operands[numberInInstruction];
		} catch (const std::logic_error &error) {
			return std::string("(error splitting operands string: ") + error.what() + ")";
		}
	}

	checkCapitalize(str);
	return str;
}

std::string Formatter::registerName(unsigned int reg) const {
	assert(capstoneInitialized);
	const char *raw = cs_reg_name(csh, reg);
	if (!raw)
		return "(invalid register)";
	std::string str(raw);
	checkCapitalize(str, false);
	return str;
}

bool is_SIMD_PS(const Operand &operand) {
#if defined(EDB_X86) || defined(EDB_X86_64)
	const Instruction &insn = *operand.owner();

	if (apriori_not_simd(insn, operand))
		return false;

	const auto number = simdOperandNormalizedNumberInInstruction(insn, operand);

	switch (insn->id) {
	case X86_INS_ADDPS:
	case X86_INS_VADDPS:
	case X86_INS_ADDSUBPS:
	case X86_INS_VADDSUBPS:
	case X86_INS_ANDNPS:
	case X86_INS_VANDNPS:
	case X86_INS_ANDPS:
	case X86_INS_VANDPS:
	case X86_INS_BLENDPS:  // imm8 isn't a SIMD reg, so ok
	case X86_INS_VBLENDPS: // imm8 isn't a SIMD reg, so ok
	case X86_INS_CMPPS:    // imm8 isn't a SIMD reg, so ok
	case X86_INS_VCMPPS:   // imm8 isn't a SIMD reg, so ok
	case X86_INS_DIVPS:
	case X86_INS_VDIVPS:
	case X86_INS_DPPS:       // imm8 isn't a SIMD reg, so ok
	case X86_INS_VDPPS:      // imm8 isn't a SIMD reg, so ok
	case X86_INS_INSERTPS:   // imm8 isn't a SIMD reg, so ok
	case X86_INS_VINSERTPS:  // imm8 isn't a SIMD reg, so ok
	case X86_INS_EXTRACTPS:  // imm8 isn't a SIMD reg, so ok
	case X86_INS_VEXTRACTPS: // imm8 isn't a SIMD reg, so ok
	case X86_INS_MOVAPS:
	case X86_INS_VMOVAPS:
	case X86_INS_ORPS:
	case X86_INS_VORPS:
	case X86_INS_XORPS:
	case X86_INS_VXORPS:
	case X86_INS_HADDPS:
	case X86_INS_VHADDPS:
	case X86_INS_HSUBPS:
	case X86_INS_VHSUBPS:
	case X86_INS_MAXPS:
	case X86_INS_VMAXPS:
	case X86_INS_MINPS:
	case X86_INS_VMINPS:
	case X86_INS_MOVHLPS:
	case X86_INS_VMOVHLPS:
	case X86_INS_MOVHPS:
	case X86_INS_VMOVHPS:
	case X86_INS_MOVLHPS:
	case X86_INS_VMOVLHPS:
	case X86_INS_MOVLPS:
	case X86_INS_VMOVLPS:
	case X86_INS_MOVMSKPS:  // GPR isn't a SIMD reg, so ok
	case X86_INS_VMOVMSKPS: // GPR isn't a SIMD reg, so ok
	case X86_INS_MOVNTPS:
	case X86_INS_VMOVNTPS:
	case X86_INS_MOVUPS:
	case X86_INS_VMOVUPS:
	case X86_INS_MULPS:
	case X86_INS_VMULPS:
	case X86_INS_RCPPS:
	case X86_INS_VRCPPS:
	case X86_INS_ROUNDPS:  // imm8 isn't a SIMD reg, so ok
	case X86_INS_VROUNDPS: // imm8 isn't a SIMD reg, so ok
	case X86_INS_RSQRTPS:
	case X86_INS_VRSQRTPS:
	case X86_INS_SHUFPS:  // imm8 isn't a SIMD reg, so ok
	case X86_INS_VSHUFPS: // imm8 isn't a SIMD reg, so ok
	case X86_INS_SQRTPS:
	case X86_INS_VSQRTPS:
	case X86_INS_SUBPS:
	case X86_INS_VSUBPS:
	case X86_INS_UNPCKHPS:
	case X86_INS_VUNPCKHPS:
	case X86_INS_UNPCKLPS:
	case X86_INS_VUNPCKLPS:
	case X86_INS_VBLENDMPS:
#if CS_API_MAJOR >= 4
	case X86_INS_VCOMPRESSPS:
	case X86_INS_VEXP2PS:
	case X86_INS_VEXPANDPS:
#endif
	case X86_INS_VFMADD132PS:
	case X86_INS_VFMADD213PS:
	case X86_INS_VFMADD231PS:
	case X86_INS_VFMADDPS: // FMA4 (AMD). XMM/YMM/mem operands only, ok
	case X86_INS_VFMADDSUB132PS:
	case X86_INS_VFMADDSUB213PS:
	case X86_INS_VFMADDSUB231PS:
	case X86_INS_VFMADDSUBPS:
	case X86_INS_VFMSUB132PS:
	case X86_INS_VFMSUBADD132PS:
	case X86_INS_VFMSUBADD213PS:
	case X86_INS_VFMSUBADD231PS:
	case X86_INS_VFMSUBADDPS: // FMA4 (AMD). Seems to have only XMM/YMM/mem operands (?)
	case X86_INS_VFMSUB213PS:
	case X86_INS_VFMSUB231PS:
	case X86_INS_VFMSUBPS: // FMA4?
	case X86_INS_VFNMADD132PS:
	case X86_INS_VFNMADD213PS:
	case X86_INS_VFNMADD231PS:
	case X86_INS_VFNMADDPS: // FMA4?
	case X86_INS_VFNMSUB132PS:
	case X86_INS_VFNMSUB213PS:
	case X86_INS_VFNMSUB231PS:
	case X86_INS_VFNMSUBPS: // FMA4?
	case X86_INS_VFRCZPS:
	case X86_INS_VRCP14PS:
	case X86_INS_VRCP28PS:
	case X86_INS_VRNDSCALEPS:
	case X86_INS_VRSQRT14PS:
	case X86_INS_VRSQRT28PS:
	case X86_INS_VTESTPS:
		return true;

	case X86_INS_VGATHERDPS:
	case X86_INS_VGATHERQPS:
		// second operand is VSIB, it's not quite PS;
		// third operand, if present (VEX-encoded version) is mask
		return number == 0;
	case X86_INS_VGATHERPF0DPS:
	case X86_INS_VGATHERPF0QPS:
	case X86_INS_VGATHERPF1DPS:
	case X86_INS_VGATHERPF1QPS:
	case X86_INS_VSCATTERPF0DPS:
	case X86_INS_VSCATTERPF0QPS:
	case X86_INS_VSCATTERPF1DPS:
	case X86_INS_VSCATTERPF1QPS:
		return false; // VSIB is not quite PS
	case X86_INS_VSCATTERDPS:
	case X86_INS_VSCATTERQPS:
		return number == 1; // first operand is VSIB, it's not quite PS

	case X86_INS_CVTDQ2PS:
	case X86_INS_VCVTDQ2PS:
	case X86_INS_CVTPD2PS:
	case X86_INS_VCVTPD2PS:
	case X86_INS_CVTPI2PS:
	case X86_INS_VCVTPH2PS:
	case X86_INS_VCVTUDQ2PS:
		return number == 0;
	case X86_INS_CVTPS2DQ:
	case X86_INS_VCVTPS2DQ:
	case X86_INS_CVTPS2PD:
	case X86_INS_VCVTPS2PD:
	case X86_INS_CVTPS2PI:
	case X86_INS_VCVTPS2PH:
	// case X86_INS_VCVTPS2QQ: // FIXME: Capstone seems to not support it
	case X86_INS_VCVTPS2UDQ:
		// case X86_INS_VCVTPS2UQQ: // FIXME: Capstone seems to not support it
		return number == 1;
	case X86_INS_BLENDVPS: // third operand (<XMM0> in docs) is mask
		return number != 2;
	case X86_INS_VBLENDVPS: // fourth operand (xmm4 in docs) is mask
		return number != 3;
	case X86_INS_VMASKMOVPS: // second (but not third) operand is mask
		return number != 1;
	case X86_INS_VPERMI2PS: // first operand is indices
		return number != 0;
	case X86_INS_VPERMILPS: // third operand is control (can be [xyz]mm register or imm8)
		return number != 2;
	case X86_INS_VPERMT2PS: // second operand is indices
	case X86_INS_VPERMPS:   // second operand is indices
		return number != 1;
	case X86_INS_VPERMIL2PS: // XOP (AMD). Fourth operand is selector
		return number != 3;
	case X86_INS_VRCPSS:
	case X86_INS_VRCP14SS:
	case X86_INS_VRCP28SS:
	case X86_INS_VROUNDSS:
	case X86_INS_VRSQRTSS:
	case X86_INS_VSQRTSS:
	case X86_INS_VRNDSCALESS:
	case X86_INS_VRSQRT14SS:
	case X86_INS_VRSQRT28SS:
	case X86_INS_VCVTSD2SS:
	case X86_INS_VCVTSI2SS:
	case X86_INS_VCVTUSI2SS:
		// These are SS, but high PS are copied from second operand to first.
		// I.e. second operand is PS, and thus first one (destination) is also PS.
		// Only third operand is actually SS.
		return number < 2;
	case X86_INS_VBROADCASTSS: // dest is PS, src is SS
		return number == 0;

	default:
		return false;
	}
#else
	(void)operand;
	return false;
#endif
}

bool is_SIMD_PD(const Operand &operand) {

#if defined(EDB_X86) || defined(EDB_X86_64)
	const Instruction &insn = *operand.owner();

	if (apriori_not_simd(insn, operand))
		return false;

	const auto number = simdOperandNormalizedNumberInInstruction(insn, operand);

	switch (insn->id) {
	case X86_INS_ADDPD:
	case X86_INS_VADDPD:
	case X86_INS_ADDSUBPD:
	case X86_INS_VADDSUBPD:
	case X86_INS_ANDNPD:
	case X86_INS_VANDNPD:
	case X86_INS_ANDPD:
	case X86_INS_VANDPD:
	case X86_INS_BLENDPD:  // imm8 isn't a SIMD reg, so ok
	case X86_INS_VBLENDPD: // imm8 isn't a SIMD reg, so ok
	case X86_INS_CMPPD:    // imm8 isn't a SIMD reg, so ok
	case X86_INS_VCMPPD:   // imm8 isn't a SIMD reg, so ok
	case X86_INS_DIVPD:
	case X86_INS_VDIVPD:
	case X86_INS_DPPD:  // imm8 isn't a SIMD reg, so ok
	case X86_INS_VDPPD: // imm8 isn't a SIMD reg, so ok
	case X86_INS_MOVAPD:
	case X86_INS_VMOVAPD:
	case X86_INS_ORPD:
	case X86_INS_VORPD:
	case X86_INS_XORPD:
	case X86_INS_VXORPD:
	case X86_INS_HADDPD:
	case X86_INS_VHADDPD:
	case X86_INS_HSUBPD:
	case X86_INS_VHSUBPD:
	case X86_INS_MAXPD:
	case X86_INS_VMAXPD:
	case X86_INS_MINPD:
	case X86_INS_VMINPD:
	case X86_INS_MOVHPD:
	case X86_INS_VMOVHPD:
	case X86_INS_MOVLPD:
	case X86_INS_VMOVLPD:
	case X86_INS_MOVMSKPD:  // GPR isn't a SIMD reg, so ok
	case X86_INS_VMOVMSKPD: // GPR isn't a SIMD reg, so ok
	case X86_INS_MOVNTPD:
	case X86_INS_VMOVNTPD:
	case X86_INS_MOVUPD:
	case X86_INS_VMOVUPD:
	case X86_INS_MULPD:
	case X86_INS_VMULPD:
	case X86_INS_ROUNDPD:  // imm8 isn't a SIMD reg, so ok
	case X86_INS_VROUNDPD: // imm8 isn't a SIMD reg, so ok
	case X86_INS_SHUFPD:   // imm8 isn't a SIMD reg, so ok
	case X86_INS_VSHUFPD:  // imm8 isn't a SIMD reg, so ok
	case X86_INS_SQRTPD:
	case X86_INS_VSQRTPD:
	case X86_INS_SUBPD:
	case X86_INS_VSUBPD:
	case X86_INS_UNPCKHPD:
	case X86_INS_VUNPCKHPD:
	case X86_INS_UNPCKLPD:
	case X86_INS_VUNPCKLPD:
	case X86_INS_VBLENDMPD:
#if CS_API_MAJOR >= 4
	case X86_INS_VCOMPRESSPD:
	case X86_INS_VEXP2PD:
	case X86_INS_VEXPANDPD:
#endif
	case X86_INS_VFMADD132PD:
	case X86_INS_VFMADD213PD:
	case X86_INS_VFMADD231PD:
	case X86_INS_VFMADDPD: // FMA4 (AMD). XMM/YMM/mem operands only (?)
	case X86_INS_VFMADDSUB132PD:
	case X86_INS_VFMADDSUB213PD:
	case X86_INS_VFMADDSUB231PD:
	case X86_INS_VFMADDSUBPD:
	case X86_INS_VFMSUB132PD:
	case X86_INS_VFMSUBADD132PD:
	case X86_INS_VFMSUBADD213PD:
	case X86_INS_VFMSUBADD231PD:
	case X86_INS_VFMSUBADDPD: // FMA4 (AMD). Seems to have only XMM/YMM/mem operands (?)
	case X86_INS_VFMSUBPD:    // FMA4?
	case X86_INS_VFMSUB213PD:
	case X86_INS_VFMSUB231PD:
	case X86_INS_VFNMADD132PD:
	case X86_INS_VFNMADDPD: // FMA4?
	case X86_INS_VFNMADD213PD:
	case X86_INS_VFNMADD231PD:
	case X86_INS_VFNMSUB132PD:
	case X86_INS_VFNMSUBPD: // FMA4?
	case X86_INS_VFNMSUB213PD:
	case X86_INS_VFNMSUB231PD:
	case X86_INS_VFRCZPD:
	case X86_INS_VRCP14PD:
	case X86_INS_VRCP28PD:
	case X86_INS_VRNDSCALEPD:
	case X86_INS_VRSQRT14PD:
	case X86_INS_VRSQRT28PD:
	case X86_INS_VTESTPD:
		return true;

	case X86_INS_VGATHERDPD:
	case X86_INS_VGATHERQPD:
		// second operand is VSIB, it's not quite PD;
		// third operand, if present (VEX-encoded version) is mask
		return number == 0;
	case X86_INS_VGATHERPF0DPD:
	case X86_INS_VGATHERPF0QPD:
	case X86_INS_VGATHERPF1DPD:
	case X86_INS_VGATHERPF1QPD:
	case X86_INS_VSCATTERPF0DPD:
	case X86_INS_VSCATTERPF0QPD:
	case X86_INS_VSCATTERPF1DPD:
	case X86_INS_VSCATTERPF1QPD:
		return false; // VSIB is not quite PD
	case X86_INS_VSCATTERDPD:
	case X86_INS_VSCATTERQPD:
		return number == 1; // first operand is VSIB, it's not quite PD

	case X86_INS_CVTDQ2PD:
	case X86_INS_VCVTDQ2PD:
	case X86_INS_CVTPS2PD:
	case X86_INS_VCVTPS2PD:
	case X86_INS_CVTPI2PD:
	case X86_INS_VCVTUDQ2PD:
		return number == 0;
	case X86_INS_CVTPD2DQ:
	case X86_INS_VCVTPD2DQ:
	case X86_INS_CVTPD2PI:
	case X86_INS_CVTPD2PS:
	case X86_INS_VCVTPD2PS:
	case X86_INS_VCVTPD2DQX: // FIXME: what's this?
	case X86_INS_VCVTPD2PSX: // FIXME: what's this?
	// case X86_INS_VCVTPD2QQ: // FIXME: Capstone seems to not support it
	case X86_INS_VCVTPD2UDQ:
		// case X86_INS_VCVTPD2UQQ: // FIXME: Capstone seems to not support it
		return number == 1;
	case X86_INS_BLENDVPD: // third operand is mask
		return number != 2;
	case X86_INS_VBLENDVPD: // fourth operand is mask
		return number != 3;
	case X86_INS_VMASKMOVPD: // second (but not third) operand is mask
		return number != 1;
	case X86_INS_VPERMI2PD: // first operand is indices
		return number != 0;
	case X86_INS_VPERMT2PD: // second operand is indices
		return number != 1;
	case X86_INS_VPERMILPD: // third operand is control (can be [xyz]mm register or imm8)
		return number != 2;
	case X86_INS_VPERMPD: // if third operand is not imm8, then second is indices (always in VPERMPS)
		assert(insn.operandCount() == 3);
		if (insn[2]->type != X86_OP_IMM)
			return number != 1;
		else
			return true;
	case X86_INS_VPERMIL2PD: // XOP (AMD). Fourth operand is selector (?)
		return number != 3;
	case X86_INS_VRCP14SD:
	case X86_INS_VRCP28SD:
	case X86_INS_VROUNDSD:
	case X86_INS_VSQRTSD:
	case X86_INS_VRNDSCALESD:
	case X86_INS_VRSQRT14SD:
	case X86_INS_VRSQRT28SD:
	case X86_INS_VCVTSS2SD:
	case X86_INS_VCVTSI2SD:
	case X86_INS_VCVTUSI2SD:
		// These are SD, but high PD are copied from second operand to first.
		// I.e. second operand is PD, and thus first one (destination) is also PD.
		// Only third operand is actually SD.
		return number < 2;
	case X86_INS_VBROADCASTSD: // dest is PD, src is SD
		return number == 0;
	default:
		return false;
	}
#else
	(void)operand;
	return false;
#endif
}

bool is_SIMD_SS(const Operand &operand) {
#if defined(EDB_X86) || defined(EDB_X86_64)
	const Instruction &insn = *operand.owner();

	if (apriori_not_simd(insn, operand))
		return false;

	const auto number = simdOperandNormalizedNumberInInstruction(insn, operand);

	switch (insn->id) {
	case X86_INS_ADDSS:
	case X86_INS_VADDSS:
	case X86_INS_CMPSS:  // imm8 isn't a SIMD reg, so ok
	case X86_INS_VCMPSS: // imm8 isn't a SIMD reg, so ok
	case X86_INS_COMISS:
	case X86_INS_VCOMISS:
	case X86_INS_DIVSS:
	case X86_INS_VDIVSS:
	case X86_INS_UCOMISS:
	case X86_INS_VUCOMISS:
	case X86_INS_MAXSS:
	case X86_INS_VMAXSS:
	case X86_INS_MINSS:
	case X86_INS_VMINSS:
	case X86_INS_MOVNTSS: // SSE4a (AMD)
	case X86_INS_MOVSS:
	case X86_INS_VMOVSS:
	case X86_INS_MULSS:
	case X86_INS_VMULSS:
	case X86_INS_RCPSS:   // SS, unlike VEX-encoded version
	case X86_INS_RSQRTSS: // SS, unlike VEX-encoded version
	case X86_INS_ROUNDSS: // imm8 isn't a SIMD reg, so ok
	case X86_INS_SQRTSS:  // SS, unlike VEX-encoded version
	case X86_INS_SUBSS:
	case X86_INS_VSUBSS:
	case X86_INS_VFMADD213SS:
	case X86_INS_VFMADD132SS:
	case X86_INS_VFMADD231SS:
	case X86_INS_VFMADDSS: // AMD?
	case X86_INS_VFMSUB213SS:
	case X86_INS_VFMSUB132SS:
	case X86_INS_VFMSUB231SS:
	case X86_INS_VFMSUBSS: // AMD?
	case X86_INS_VFNMADD213SS:
	case X86_INS_VFNMADD132SS:
	case X86_INS_VFNMADD231SS:
	case X86_INS_VFNMADDSS: // AMD?
	case X86_INS_VFNMSUB213SS:
	case X86_INS_VFNMSUB132SS:
	case X86_INS_VFNMSUB231SS:
	case X86_INS_VFNMSUBSS: // AMD?
	case X86_INS_VFRCZSS:   // AMD?
		return true;

	case X86_INS_VCVTSS2SD:
		return number == 2;
	case X86_INS_CVTSS2SD:
	case X86_INS_CVTSS2SI:
	case X86_INS_VCVTSS2SI:
	case X86_INS_VCVTSS2USI:
		return number == 1;
	case X86_INS_CVTSD2SS: // SS, unlike VEX-encoded version
	case X86_INS_CVTSI2SS: // SS, unlike VEX-encoded version
		return number == 0;
	case X86_INS_VCVTSD2SS:
	case X86_INS_VCVTSI2SS:
	case X86_INS_VCVTUSI2SS:
		// These instructions are SS, but high PS are copied from second operand to first,
		// so second operand is PS, thus first too. Third operand is not SS by its meaning.
		return false;
	case X86_INS_VRCPSS:
	case X86_INS_VRCP14SS:
	case X86_INS_VRCP28SS:
	case X86_INS_VROUNDSS:
	case X86_INS_VRSQRTSS:
	case X86_INS_VSQRTSS:
	case X86_INS_VRNDSCALESS:
	case X86_INS_VRSQRT14SS:
	case X86_INS_VRSQRT28SS:
		// These are SS, but high PS are copied from second operand to first.
		// I.e. second operand is PS, and thus first one (destination) is also PS.
		// Only third operand is actually SS.
		return number == 2;
	case X86_INS_VBROADCASTSS: // dest is PS, src is SS
		return number == 1;
	default:
		return false;
	}
#else
	(void)operand;
	return false;
#endif
}

bool is_SIMD_SD(const Operand &operand) {

#if defined(EDB_X86) || defined(EDB_X86_64)
	const Instruction &insn = *operand.owner();

	if (apriori_not_simd(insn, operand))
		return false;

	const auto number = simdOperandNormalizedNumberInInstruction(insn, operand);

	switch (insn->id) {
	case X86_INS_ADDSD:
	case X86_INS_VADDSD:
	case X86_INS_CMPSD:  // imm8 isn't a SIMD reg, so ok
	case X86_INS_VCMPSD: // imm8 isn't a SIMD reg, so ok
	case X86_INS_COMISD:
	case X86_INS_VCOMISD:
	case X86_INS_DIVSD:
	case X86_INS_VDIVSD:
	case X86_INS_UCOMISD:
	case X86_INS_VUCOMISD:
	case X86_INS_MAXSD:
	case X86_INS_VMAXSD:
	case X86_INS_MINSD:
	case X86_INS_VMINSD:
	case X86_INS_MOVNTSD: // SSE4a (AMD)
	case X86_INS_MOVSD:
	case X86_INS_VMOVSD:
	case X86_INS_MULSD:
	case X86_INS_VMULSD:
	case X86_INS_ROUNDSD: // imm8 isn't a SIMD reg, so ok
	case X86_INS_SQRTSD:  // SD, unlike VEX-encoded version
	case X86_INS_SUBSD:
	case X86_INS_VSUBSD:
	case X86_INS_VFMADD213SD:
	case X86_INS_VFMADD132SD:
	case X86_INS_VFMADD231SD:
	case X86_INS_VFMADDSD: // AMD?
	case X86_INS_VFMSUB213SD:
	case X86_INS_VFMSUB132SD:
	case X86_INS_VFMSUB231SD:
	case X86_INS_VFMSUBSD: // AMD?
	case X86_INS_VFNMADD213SD:
	case X86_INS_VFNMADD132SD:
	case X86_INS_VFNMADD231SD:
	case X86_INS_VFNMADDSD: // AMD?
	case X86_INS_VFNMSUB213SD:
	case X86_INS_VFNMSUB132SD:
	case X86_INS_VFNMSUB231SD:
	case X86_INS_VFNMSUBSD: // AMD?
	case X86_INS_VFRCZSD:   // AMD?
		return true;

	case X86_INS_VCVTSD2SS:
		return number == 2;
	case X86_INS_CVTSD2SS:
	case X86_INS_CVTSD2SI:
	case X86_INS_VCVTSD2SI:
	case X86_INS_VCVTSD2USI:
		return number == 1;
	case X86_INS_CVTSS2SD: // SD, unlike VEX-encoded version
	case X86_INS_CVTSI2SD: // SD, unlike VEX-encoded version
		return number == 0;
	case X86_INS_VCVTSS2SD:
	case X86_INS_VCVTSI2SD:
	case X86_INS_VCVTUSI2SD:
		// These instructions are SD, but high PD are copied from second operand to first,
		// so second operand is PD, thus first too. Third operand is not SD by its meaning.
		return false;
	case X86_INS_VRCP14SD:
	case X86_INS_VRCP28SD:
	case X86_INS_VROUNDSD:
	case X86_INS_VSQRTSD:
	case X86_INS_VRNDSCALESD:
	case X86_INS_VRSQRT14SD:
	case X86_INS_VRSQRT28SD:
		// These are SD, but high PD are copied from second operand to first.
		// I.e. second operand is PD, and thus first one (destination) is also PD.
		// Only third operand is actually SD.
		return number == 2;
	case X86_INS_VBROADCASTSD: // dest is PD, src is SD
		return number == 1;
	default:
		return false;
	}
#else
	(void)operand;
	return false;
#endif
}

bool is_SIMD_SI(const Operand &operand) {

#if defined(EDB_X86) || defined(EDB_X86_64)
	const Instruction &insn = *operand.owner();
	const auto number       = simdOperandNormalizedNumberInInstruction(insn, operand, true);

	switch (insn->id) {
	case X86_INS_VCVTSI2SS:
	case X86_INS_VCVTSI2SD:
		return number == 2;
	case X86_INS_CVTSI2SS:
	case X86_INS_CVTSI2SD:
		return number == 1;
	case X86_INS_CVTSS2SI:
	case X86_INS_VCVTSS2SI:
	case X86_INS_CVTSD2SI:
	case X86_INS_VCVTSD2SI:
		return number == 0;
	default:
		return false;
	}
#else
	(void)operand;
	return false;
#endif
}

bool is_SIMD_USI(const Operand &operand) {
#if defined(EDB_X86) || defined(EDB_X86_64)
	const Instruction &insn = *operand.owner();
	const auto number       = simdOperandNormalizedNumberInInstruction(insn, operand, true);

	switch (insn->id) {
	case X86_INS_VCVTUSI2SS:
	case X86_INS_VCVTUSI2SD:
		return number == 2;
	case X86_INS_VCVTSS2USI:
	case X86_INS_VCVTSD2USI:
		return number == 0;
	default:
		return false;
	}
#else
	(void)operand;
	return false;
#endif
}

bool is_return(const Instruction &insn) {
	if (!insn) return false;
	return cs_insn_group(csh, insn.native(), CS_GRP_RET);
}

bool is_jump(const Instruction &insn) {
	if (!insn) return false;
	return cs_insn_group(csh, insn.native(), CS_GRP_JUMP);
}

bool is_call(const Instruction &insn) {
	if (!insn) return false;
	return cs_insn_group(csh, insn.native(), CS_GRP_CALL);
}

bool modifies_pc(const Instruction &insn) {
	if (is_call(insn) || is_jump(insn) || is_interrupt(insn))
		return true;
#if defined(EDB_X86) || defined(EDB_X86_64)
	return false;
#elif defined(EDB_ARM32)
	const auto &detail = *insn->detail;
	for (uint8_t i = 0; i < detail.regs_write_count; ++i)
		if (detail.regs_write[i] == ARM_REG_PC)
			return true;
	const auto &arm = detail.arm;
	for (uint8_t i = 0; i < arm.op_count; ++i) {
		const auto &op = arm.operands[i];
#if CS_API_MAJOR >= 4
		if (op.access == CS_AC_WRITE && op.type == CS_OP_REG && op.reg == ARM_REG_PC)
			return true;
#endif
		if (op.type == ARM_OP_MEM && insn.native()->detail->arm.writeback && op.mem.base == ARM_REG_PC)
			return true;
	}
	return false;
#else
#error "Not implemented"
#endif
}

}
