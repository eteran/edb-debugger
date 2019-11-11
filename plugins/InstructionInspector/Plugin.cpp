/*
Copyright (C) 2016 Ruslan Kabatsayev <b7.10110111@gmail.com>

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

#include "Plugin.h"
#include "Configuration.h"
#include "IDebugger.h"
#include "Util.h"
#include "edb.h"

#include <QDebug>
#include <QDir>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSplitter>
#include <QTemporaryFile>
#include <QTextBrowser>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QtGlobal>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <iomanip>
#include <map>
#include <sstream>
#include <utility>

#ifdef Q_OS_UNIX
#include <elf.h>
// TODO(eteran): refactor this in terms of "libELF" so we are more portable
#endif

#include <capstone/capstone.h>

namespace InstructionInspector {
namespace {

struct NormalizeFailure {};

/**
 * @brief printBytes
 * @param ptr
 * @param size
 * @param printZeros
 * @return
 */
std::string printBytes(const void *ptr, std::size_t size, bool printZeros = true) {

	std::ostringstream str;
	str << std::setfill('0') << std::uppercase << std::hex;
	const auto bytes = reinterpret_cast<const unsigned char *>(ptr);

	for (std::size_t i = 0; i < size; ++i) {
		if (!str.str().empty()) {
			str << ' ';
		}

		if (bytes[i] || printZeros) {
			str << std::setw(2) << static_cast<unsigned>(bytes[i]);
		}
	}

	return str.str();
}

/**
 * @brief toHex
 * @param x
 * @param Signed
 * @return
 */
std::string toHex(unsigned long long x, bool Signed = false) {
	bool negative = false;

	if (Signed && static_cast<long long>(x) < 0) {
		x = ~x + 1;

		if (~x + 1 != x) {
			negative = true;
		}
	}

	std::ostringstream str;
	str << std::hex << (negative ? "-" : "") << "0x" << std::uppercase << x;
	return str.str();
}

#if defined(EDB_ARM32)
/**
 * @brief toFloatString
 * @param x
 * @return
 */
std::string toFloatString(double x) {
	std::ostringstream str;
	str.precision(17);
	str << x;
	return str.str();
}
#endif

/**
 * @brief uppercase
 * @param text
 * @return
 */
std::string uppercase(std::string text) {
	std::transform(text.begin(), text.end(), text.begin(), [](int ch) { return std::toupper(ch); });
	return text;
}

/**
 * @brief getGroupNames
 * @param csh
 * @param insn_
 * @return
 */
std::vector<std::string> getGroupNames(csh csh, const cs_insn *insn_) {

	std::vector<std::string> groupNames;
	for (int g = 0; g < insn_->detail->groups_count; ++g) {
		const auto grp = insn_->detail->groups[g];

		if (!grp) {
			groupNames.emplace_back("INVALID");
			continue;
		}

		if (const auto groupName = cs_group_name(csh, grp)) {
			groupNames.emplace_back(uppercase(groupName));
		} else {
			groupNames.emplace_back(toHex(grp));
		}
	}

	return groupNames;
}

/**
 * @brief printReg
 * @param csh
 * @param reg
 * @param canBeZero
 * @return
 */
std::string printReg(csh csh, int reg, bool canBeZero = false) {
	if (!reg) {
		return canBeZero ? "" : "INVALID";
	}

	if (const auto regName = cs_reg_name(csh, reg)) {
		return uppercase(regName);
	} else {
		return toHex(reg);
	}
}

#if 0
/**
 * @brief printRegs
 * @param csh
 * @param regsBuffer
 * @param size
 * @return
 */
std::string printRegs(csh csh, const uint16_t *regsBuffer, std::size_t size) {

	std::ostringstream str;
	for (std::size_t r = 0; r < size; ++r) {
		if (!str.str().empty()) {
			str << ",";
		}

		str << printReg(csh, regsBuffer[r]);
	}

	const std::string string = str.str();

	if (string.empty()) {
		return "(none)";
	}

	return string;
}
#endif

#if CS_API_MAJOR >= 4
/**
 * @brief printXOP_CC
 * @param cc
 * @return
 */
std::string printXOP_CC(x86_xop_cc cc) {
	static const std::map<x86_xop_cc, const char *> codes{
		{X86_XOP_CC_INVALID, "INVALID"},
		{X86_XOP_CC_LT, "LT"},
		{X86_XOP_CC_LE, "LE"},
		{X86_XOP_CC_GT, "GT"},
		{X86_XOP_CC_GE, "GE"},
		{X86_XOP_CC_EQ, "EQ"},
		{X86_XOP_CC_NEQ, "NEQ"},
		{X86_XOP_CC_FALSE, "FALSE"},
		{X86_XOP_CC_TRUE, "TRUE"},
	};

	const auto found = codes.find(cc);
	if (found == codes.end()) {
		return toHex(cc);
	}
	return found->second;
}
#endif

/**
 * @brief printSSE_CC
 * @param cc
 * @return
 */
std::string printSSE_CC(x86_sse_cc cc) {
	static const std::map<x86_sse_cc, const char *> codes{
		{X86_SSE_CC_INVALID, "INVALID"},
		{X86_SSE_CC_EQ, "EQ"},
		{X86_SSE_CC_LT, "LT"},
		{X86_SSE_CC_LE, "LE"},
		{X86_SSE_CC_UNORD, "UNORD"},
		{X86_SSE_CC_NEQ, "NEQ"},
		{X86_SSE_CC_NLT, "NLT"},
		{X86_SSE_CC_NLE, "NLE"},
		{X86_SSE_CC_ORD, "ORD"},
	};

	const auto found = codes.find(cc);
	if (found == codes.end()) {
		return toHex(cc);
	}

	return found->second;
}

/**
 * @brief printAVX_CC
 * @param cc
 * @return
 */
std::string printAVX_CC(x86_avx_cc cc) {

	static const std::map<x86_avx_cc, const char *> codes{
		{X86_AVX_CC_INVALID, "INVALID"},
		{X86_AVX_CC_EQ, "EQ"},
		{X86_AVX_CC_LT, "LT"},
		{X86_AVX_CC_LE, "LE"},
		{X86_AVX_CC_UNORD, "UNORD"},
		{X86_AVX_CC_NEQ, "NEQ"},
		{X86_AVX_CC_NLT, "NLT"},
		{X86_AVX_CC_NLE, "NLE"},
		{X86_AVX_CC_ORD, "ORD"},
		{X86_AVX_CC_EQ_UQ, "EQ_UQ"},
		{X86_AVX_CC_NGE, "NGE"},
		{X86_AVX_CC_NGT, "NGT"},
		{X86_AVX_CC_FALSE, "FALSE"},
		{X86_AVX_CC_NEQ_OQ, "NEQ_OQ"},
		{X86_AVX_CC_GE, "GE"},
		{X86_AVX_CC_GT, "GT"},
		{X86_AVX_CC_TRUE, "TRUE"},
		{X86_AVX_CC_EQ_OS, "EQ_OS"},
		{X86_AVX_CC_LT_OQ, "LT_OQ"},
		{X86_AVX_CC_LE_OQ, "LE_OQ"},
		{X86_AVX_CC_UNORD_S, "UNORD_S"},
		{X86_AVX_CC_NEQ_US, "NEQ_US"},
		{X86_AVX_CC_NLT_UQ, "NLT_UQ"},
		{X86_AVX_CC_NLE_UQ, "NLE_UQ"},
		{X86_AVX_CC_ORD_S, "ORD_S"},
		{X86_AVX_CC_EQ_US, "EQ_US"},
		{X86_AVX_CC_NGE_UQ, "NGE_UQ"},
		{X86_AVX_CC_NGT_UQ, "NGT_UQ"},
		{X86_AVX_CC_FALSE_OS, "FALSE_OS"},
		{X86_AVX_CC_NEQ_OS, "NEQ_OS"},
		{X86_AVX_CC_GE_OQ, "GE_OQ"},
		{X86_AVX_CC_GT_OQ, "GT_OQ"},
		{X86_AVX_CC_TRUE_US, "TRUE_US"},
	};

	const auto found = codes.find(cc);
	if (found == codes.end()) {
		return toHex(cc);
	}

	return found->second;
}

/**
 * @brief printAVX_RM
 * @param cc
 * @return
 */
std::string printAVX_RM(x86_avx_rm cc) {
	static const std::map<x86_avx_rm, const char *> codes{
		{X86_AVX_RM_INVALID, "invalid"},
		{X86_AVX_RM_RN, "to nearest"},
		{X86_AVX_RM_RD, "down"},
		{X86_AVX_RM_RU, "up"},
		{X86_AVX_RM_RZ, "toward zero"},
	};

	const auto found = codes.find(cc);
	if (found == codes.end()) {
		return toHex(cc);
	}

	return found->second;
}

#if CS_API_MAJOR >= 4
/**
 * @brief getChangedEFLAGSNames
 * @param efl
 * @return
 */
std::vector<std::string> getChangedEFLAGSNames(std::uint64_t efl) {
	std::vector<std::string> flags;

	if (efl & X86_EFLAGS_MODIFY_AF) {
		flags.emplace_back("MODIFY_AF");
		efl &= ~X86_EFLAGS_MODIFY_AF;
	}
	if (efl & X86_EFLAGS_MODIFY_CF) {
		flags.emplace_back("MODIFY_CF");
		efl &= ~X86_EFLAGS_MODIFY_CF;
	}
	if (efl & X86_EFLAGS_MODIFY_SF) {
		flags.emplace_back("MODIFY_SF");
		efl &= ~X86_EFLAGS_MODIFY_SF;
	}
	if (efl & X86_EFLAGS_MODIFY_ZF) {
		flags.emplace_back("MODIFY_ZF");
		efl &= ~X86_EFLAGS_MODIFY_ZF;
	}
	if (efl & X86_EFLAGS_MODIFY_PF) {
		flags.emplace_back("MODIFY_PF");
		efl &= ~X86_EFLAGS_MODIFY_PF;
	}
	if (efl & X86_EFLAGS_MODIFY_OF) {
		flags.emplace_back("MODIFY_OF");
		efl &= ~X86_EFLAGS_MODIFY_OF;
	}
	if (efl & X86_EFLAGS_MODIFY_TF) {
		flags.emplace_back("MODIFY_TF");
		efl &= ~X86_EFLAGS_MODIFY_TF;
	}
	if (efl & X86_EFLAGS_MODIFY_IF) {
		flags.emplace_back("MODIFY_IF");
		efl &= ~X86_EFLAGS_MODIFY_IF;
	}
	if (efl & X86_EFLAGS_MODIFY_DF) {
		flags.emplace_back("MODIFY_DF");
		efl &= ~X86_EFLAGS_MODIFY_DF;
	}
	if (efl & X86_EFLAGS_MODIFY_NT) {
		flags.emplace_back("MODIFY_NT");
		efl &= ~X86_EFLAGS_MODIFY_NT;
	}
	if (efl & X86_EFLAGS_MODIFY_RF) {
		flags.emplace_back("MODIFY_RF");
		efl &= ~X86_EFLAGS_MODIFY_RF;
	}
	if (efl & X86_EFLAGS_PRIOR_OF) {
		flags.emplace_back("PRIOR_OF");
		efl &= ~X86_EFLAGS_PRIOR_OF;
	}
	if (efl & X86_EFLAGS_PRIOR_SF) {
		flags.emplace_back("PRIOR_SF");
		efl &= ~X86_EFLAGS_PRIOR_SF;
	}
	if (efl & X86_EFLAGS_PRIOR_ZF) {
		flags.emplace_back("PRIOR_ZF");
		efl &= ~X86_EFLAGS_PRIOR_ZF;
	}
	if (efl & X86_EFLAGS_PRIOR_AF) {
		flags.emplace_back("PRIOR_AF");
		efl &= ~X86_EFLAGS_PRIOR_AF;
	}
	if (efl & X86_EFLAGS_PRIOR_PF) {
		flags.emplace_back("PRIOR_PF");
		efl &= ~X86_EFLAGS_PRIOR_PF;
	}
	if (efl & X86_EFLAGS_PRIOR_CF) {
		flags.emplace_back("PRIOR_CF");
		efl &= ~X86_EFLAGS_PRIOR_CF;
	}
	if (efl & X86_EFLAGS_PRIOR_TF) {
		flags.emplace_back("PRIOR_TF");
		efl &= ~X86_EFLAGS_PRIOR_TF;
	}
	if (efl & X86_EFLAGS_PRIOR_IF) {
		flags.emplace_back("PRIOR_IF");
		efl &= ~X86_EFLAGS_PRIOR_IF;
	}
	if (efl & X86_EFLAGS_PRIOR_DF) {
		flags.emplace_back("PRIOR_DF");
		efl &= ~X86_EFLAGS_PRIOR_DF;
	}
	if (efl & X86_EFLAGS_PRIOR_NT) {
		flags.emplace_back("PRIOR_NT");
		efl &= ~X86_EFLAGS_PRIOR_NT;
	}
	if (efl & X86_EFLAGS_RESET_OF) {
		flags.emplace_back("RESET_OF");
		efl &= ~X86_EFLAGS_RESET_OF;
	}
	if (efl & X86_EFLAGS_RESET_CF) {
		flags.emplace_back("RESET_CF");
		efl &= ~X86_EFLAGS_RESET_CF;
	}
	if (efl & X86_EFLAGS_RESET_DF) {
		flags.emplace_back("RESET_DF");
		efl &= ~X86_EFLAGS_RESET_DF;
	}
	if (efl & X86_EFLAGS_RESET_IF) {
		flags.emplace_back("RESET_IF");
		efl &= ~X86_EFLAGS_RESET_IF;
	}
	if (efl & X86_EFLAGS_RESET_SF) {
		flags.emplace_back("RESET_SF");
		efl &= ~X86_EFLAGS_RESET_SF;
	}
	if (efl & X86_EFLAGS_RESET_AF) {
		flags.emplace_back("RESET_AF");
		efl &= ~X86_EFLAGS_RESET_AF;
	}
	if (efl & X86_EFLAGS_RESET_TF) {
		flags.emplace_back("RESET_TF");
		efl &= ~X86_EFLAGS_RESET_TF;
	}
	if (efl & X86_EFLAGS_RESET_NT) {
		flags.emplace_back("RESET_NT");
		efl &= ~X86_EFLAGS_RESET_NT;
	}
	if (efl & X86_EFLAGS_RESET_PF) {
		flags.emplace_back("RESET_PF");
		efl &= ~X86_EFLAGS_RESET_PF;
	}
	if (efl & X86_EFLAGS_SET_CF) {
		flags.emplace_back("SET_CF");
		efl &= ~X86_EFLAGS_SET_CF;
	}
	if (efl & X86_EFLAGS_SET_DF) {
		flags.emplace_back("SET_DF");
		efl &= ~X86_EFLAGS_SET_DF;
	}
	if (efl & X86_EFLAGS_SET_IF) {
		flags.emplace_back("SET_IF");
		efl &= ~X86_EFLAGS_SET_IF;
	}
	if (efl & X86_EFLAGS_TEST_OF) {
		flags.emplace_back("TEST_OF");
		efl &= ~X86_EFLAGS_TEST_OF;
	}
	if (efl & X86_EFLAGS_TEST_SF) {
		flags.emplace_back("TEST_SF");
		efl &= ~X86_EFLAGS_TEST_SF;
	}
	if (efl & X86_EFLAGS_TEST_ZF) {
		flags.emplace_back("TEST_ZF");
		efl &= ~X86_EFLAGS_TEST_ZF;
	}
	if (efl & X86_EFLAGS_TEST_PF) {
		flags.emplace_back("TEST_PF");
		efl &= ~X86_EFLAGS_TEST_PF;
	}
	if (efl & X86_EFLAGS_TEST_CF) {
		flags.emplace_back("TEST_CF");
		efl &= ~X86_EFLAGS_TEST_CF;
	}
	if (efl & X86_EFLAGS_TEST_NT) {
		flags.emplace_back("TEST_NT");
		efl &= ~X86_EFLAGS_TEST_NT;
	}
	if (efl & X86_EFLAGS_TEST_DF) {
		flags.emplace_back("TEST_DF");
		efl &= ~X86_EFLAGS_TEST_DF;
	}
	if (efl & X86_EFLAGS_UNDEFINED_OF) {
		flags.emplace_back("UNDEFINED_OF");
		efl &= ~X86_EFLAGS_UNDEFINED_OF;
	}
	if (efl & X86_EFLAGS_UNDEFINED_SF) {
		flags.emplace_back("UNDEFINED_SF");
		efl &= ~X86_EFLAGS_UNDEFINED_SF;
	}
	if (efl & X86_EFLAGS_UNDEFINED_ZF) {
		flags.emplace_back("UNDEFINED_ZF");
		efl &= ~X86_EFLAGS_UNDEFINED_ZF;
	}
	if (efl & X86_EFLAGS_UNDEFINED_PF) {
		flags.emplace_back("UNDEFINED_PF");
		efl &= ~X86_EFLAGS_UNDEFINED_PF;
	}
	if (efl & X86_EFLAGS_UNDEFINED_AF) {
		flags.emplace_back("UNDEFINED_AF");
		efl &= ~X86_EFLAGS_UNDEFINED_AF;
	}
	if (efl & X86_EFLAGS_UNDEFINED_CF) {
		flags.emplace_back("UNDEFINED_CF");
		efl &= ~X86_EFLAGS_UNDEFINED_CF;
	}

	if (efl) {
		flags.emplace_back(toHex(efl));
	}
	return flags;
}
#endif

#if defined(EDB_ARM32)
/**
 * @brief printCond
 * @param cc
 * @return
 */
std::string printCond(arm_cc cc) {
	static const std::map<arm_cc, const char *> types{
		{ARM_CC_INVALID, "invalid"},
		{ARM_CC_EQ, "EQ"},
		{ARM_CC_NE, "NE"},
		{ARM_CC_HS, "HS"},
		{ARM_CC_LO, "LO"},
		{ARM_CC_MI, "MI"},
		{ARM_CC_PL, "PL"},
		{ARM_CC_VS, "VS"},
		{ARM_CC_VC, "VC"},
		{ARM_CC_HI, "HI"},
		{ARM_CC_LS, "LS"},
		{ARM_CC_GE, "GE"},
		{ARM_CC_LT, "LT"},
		{ARM_CC_GT, "GT"},
		{ARM_CC_LE, "LE"},
		{ARM_CC_AL, "AL"},
	};

	const auto found = types.find(cc);
	if (found == types.end()) {
		return toHex(cc);
	}
	return found->second;
}
#endif

/**
 * @brief printOpType
 * @param op
 * @return
 */
std::string printOpType(const x86_op_type &op) {

	static const std::map<x86_op_type, const char *> types{
		{X86_OP_INVALID, "invalid"},
		{X86_OP_REG, "register"},
		{X86_OP_IMM, "immediate"},
		{X86_OP_MEM, "memory"},
	};

	const auto found = types.find(op);
	if (found == types.end()) {
		return toHex(op);
	}
	return found->second;
}

#if defined(EDB_ARM32)
/**
 * @brief printShiftType
 * @param op
 * @return
 */
std::string printShiftType(const arm_shifter &op) {

	static const std::map<arm_shifter, const char *> types{
		{ARM_SFT_INVALID, "invalid"},
		{ARM_SFT_ASR, "ASR"},
		{ARM_SFT_LSL, "LSL"},
		{ARM_SFT_LSR, "LSR"},
		{ARM_SFT_ROR, "ROR"},
		{ARM_SFT_RRX, "RRX"},
		{ARM_SFT_ASR_REG, "ASR with register"},
		{ARM_SFT_LSL_REG, "LSL with register"},
		{ARM_SFT_LSR_REG, "LSR with register"},
		{ARM_SFT_ROR_REG, "ROR with register"},
		{ARM_SFT_RRX_REG, "RRX with register"},
	};

	const auto found = types.find(op);
	if (found == types.end()) {
		return toHex(op);
	}
	return found->second;
}

/**
 * @brief printOpType
 * @param op
 * @return
 */
std::string printOpType(const arm_setend_type &op) {

	static const std::map<arm_setend_type, const char *> types{
		{ARM_SETEND_INVALID, "invalid"},
		{ARM_SETEND_BE, "BE"},
		{ARM_SETEND_LE, "LE"},
	};

	const auto found = types.find(op);
	if (found == types.end()) {
		return toHex(op);
	}
	return found->second;
}

/**
 * @brief printOpType
 * @param op
 * @return
 */
std::string printOpType(const arm_op_type &op) {

	static const std::map<arm_op_type, const char *> types{
		{ARM_OP_INVALID, "invalid"},
		{ARM_OP_REG, "register"},
		{ARM_OP_IMM, "immediate"},
		{ARM_OP_MEM, "memory"},
		{ARM_OP_FP, "floating-point"},
		{ARM_OP_CIMM, "C-Immediate"},
		{ARM_OP_PIMM, "P-Immediate"},
		{ARM_OP_SETEND, "operand for SETEND"},
		{ARM_OP_SYSREG, "MSR/MSR special register"},
	};

	const auto found = types.find(op);
	if (found == types.end()) {
		return toHex(op);
	}
	return found->second;
}
#endif

/**
 * @brief printAVX_Bcast
 * @param bc
 * @return
 */
std::string printAVX_Bcast(x86_avx_bcast bc) {

	static const std::map<x86_avx_bcast, const char *> types{
		{X86_AVX_BCAST_INVALID, "invalid"},
		{X86_AVX_BCAST_2, "{1to2}"},
		{X86_AVX_BCAST_4, "{1to4}"},
		{X86_AVX_BCAST_8, "{1to8}"},
		{X86_AVX_BCAST_16, "{1to16}"},
	};
	const auto found = types.find(bc);
	if (found == types.end()) {
		return toHex(bc);
	}
	return found->second;
}

#if CS_API_MAJOR >= 4
/**
 * @brief printAccessMode
 * @param mode
 * @return
 */
std::string printAccessMode(unsigned mode) {
	std::ostringstream str;

	if (mode & CS_AC_READ) {
		if (!str.str().empty()) {
			str << "+";
		}
		str << "read";
		mode &= ~CS_AC_READ;
	}

	if (mode & CS_AC_WRITE) {
		if (!str.str().empty()) {
			str << "+";
		}
		str << "write";
		mode &= ~CS_AC_WRITE;
	}

	if (mode) {
		if (!str.str().empty()) {
			str << "+";
		}
		str << toHex(mode);
	}

	const std::string string = str.str();
	if (string.empty()) {
		return "none";
	}

	return string;
}
#endif

/**
 * @brief normalizeOBJDUMP
 * @param text
 * @param bits
 * @return
 */
QString normalizeOBJDUMP(const QString &text, int bits) {
	auto parts = text.split('\t');
#if defined(EDB_X86) || defined(EDB_X86_64)
	if (parts.size() != 3) return text + " ; unexpected format";
#elif defined(EDB_ARM32)
	if (parts.size() < 3) return text + " ; unexpected format";
#else
	return text + " ; WARNING: InstructionInspector's normalization is not implemented for this arch";
#endif

	auto &addr   = parts[0];
	auto &bytes  = parts[1];
	auto &disasm = parts[2];
	addr         = addr.trimmed().toUpper();

	// left removes colon
	addr   = addr.left(addr.size() - 1).rightJustified(bits / 4, '0');
	bytes  = bytes.trimmed().toUpper();
	disasm = disasm.trimmed().replace(QRegExp("  +"), " ");

#if defined(EDB_ARM32)
	// ARM objdump prints instruction bytes as a word instead of separate bytes. We won't
	// change this format, but will align the disassembly.
	if (bytes.size() > 2)
		bytes = bytes.leftJustified(bytes.size() * 3 / 2 - 1);

	// operands and comments are separated by a one or more tabs on ARM
	for (unsigned i = 3; i < parts.size(); ++i)
		disasm += " " + parts[i];
#endif
	return addr + "   " + bytes + "   " + disasm;
}

/**
 * @brief runOBJDUMP
 * @param bytes
 * @param address
 * @return
 */
std::string runOBJDUMP(const std::vector<std::uint8_t> &bytes, edb::address_t address) {

	const std::string processName = "objdump";
	const auto bits               = edb::v1::debuggeeIs32Bit() ? 32 : 64;

	QTemporaryFile binary(QDir::tempPath() + "/edb_insn_inspector_temp_XXXXXX.bin");

	if (!binary.open()) {
		return "; Failed to create binary file";
	}

	const int size = bytes.size();

	if (binary.write(reinterpret_cast<const char *>(bytes.data()), size) != size) {
		return "; Failed to write to binary file";
	}

	binary.close();
	QProcess process;
	process.start(processName.c_str(), {
		"-D",
			"--target=binary",
#if defined(EDB_X86) || defined(EDB_X86_64)
			"--insn-width=15",
			"--architecture=i386" + QString(bits == 64 ? ":x86-64" : ""),
			"-M",
			"intel,"
			"intel-mnemonic",
#elif defined(EDB_ARM32)
								   "--insn-width=4",
								   "-m",
								   "arm",
								   edb::v1::debugger_core->cpuMode() == IDebugger::CpuMode::Thumb ? "-Mforce-thumb" : "-Mno-force-thumb",
#else
#error "Not implemented"
#endif
			"--adjust-vma=" + address.toPointerString(), binary.fileName()
	});

	if (process.waitForFinished()) {
		if (process.exitCode() != 0)
			return ("; got response: \"" + process.readAllStandardError() + "\"").constData();
		if (process.exitStatus() != QProcess::NormalExit)
			return "; process crashed";

		const auto output  = QString::fromUtf8(process.readAllStandardOutput()).split('\n');
		const auto addrStr = address.toHexString().toLower().replace(QRegExp("^0+"), "");

		QString result;
		for (auto &line : output) {
			if (line.contains(QRegExp("^ *" + addrStr + ":\t[^\t]+\t"))) {
				result = line;
				break;
			}
		}

		if (result.isEmpty()) {
			// Try truncating higher bits of address: some versions of objconv can't --adjust-vma to
			// 64-bit values even with --architecture=i386:x86-64 (namely, 32-bit binutils 2.26.20160125)
			if (bits == 64 && address > UINT32_MAX) {
				return runOBJDUMP(bytes, address & UINT32_MAX) + " ; WARNING: origin had to be truncated for objdump";
			}

			return ("; failed to find disassembly. stdout: \"" + output.join("\n") + "\"").toStdString();
		}

		return normalizeOBJDUMP(result, bits).toStdString();
	} else if (process.error() == QProcess::FailedToStart) {
		return "; Failed to start " + processName;
	}

	return "; Unknown error while running " + processName;
}

#if defined(EDB_X86) || defined(EDB_X86_64)
/**
 * @brief normalizeNDISASM
 * @param text
 * @param bits
 * @return
 */
QString normalizeNDISASM(const QString &text, int bits) {

	auto lines = text.split('\n');
	Q_ASSERT(!lines.isEmpty());
	auto parts = lines.takeFirst().replace(QRegExp("  +"), "\t").split('\t');

	if (parts.size() != 3)
		return text + " ; unexpected format 1";

	auto &addr   = parts[0];
	auto &bytes  = parts[1];
	auto &disasm = parts[2];

	addr  = addr.rightJustified(bits / 4, '0');
	bytes = bytes.trimmed();

	// connect the rest of lines to bytes
	for (auto &line : lines) {
		if (!line.contains(QRegExp("^ +-[0-9a-fA-F]+$"))) {
			return text + " ; unexpected format 2";
		}

		line = line.trimmed();
		bytes += line.rightRef(line.size() - 1); // remove leading '-'
	}

	bytes.replace(QRegExp("(..)"), "\\1 ");
	return addr + "   " + bytes.trimmed() + "   " + disasm.trimmed();
}

/**
 * @brief runNDISASM
 * @param bytes
 * @param address
 * @return
 */
std::string runNDISASM(const std::vector<std::uint8_t> &bytes, edb::address_t address) {

	const std::string processName = "ndisasm";
	const auto bits               = edb::v1::debuggeeIs32Bit() ? 32 : 64;

	QTemporaryFile binary(QDir::tempPath() + "/edb_insn_inspector_temp_XXXXXX.bin");

	if (!binary.open()) {
		return "; Failed to create binary file";
	}

	const int size = bytes.size();

	if (binary.write(reinterpret_cast<const char *>(bytes.data()), size) != size) {
		return "; Failed to write to binary file";
	}

	binary.close();
	QProcess process;
	process.start(processName.c_str(), {"-o", address.toPointerString(), "-b", std::to_string(bits).c_str(), binary.fileName()});

	if (process.waitForFinished()) {
		if (process.exitCode() != 0) {
			return ("; got response: \"" + process.readAllStandardError() + "\"").constData();
		}

		if (process.exitStatus() != QProcess::NormalExit) {
			return "; process crashed";
		}

		auto output    = QString::fromUtf8(process.readAllStandardOutput()).split('\n');
		QString result = output.takeFirst();

		for (auto &line : output) {
			if (line.contains(QRegExp("^ +-[0-9a-fA-F]+$"))) {
				result += "\n" + line;
			} else {
				break;
			}
		}

		return normalizeNDISASM(result, bits).toStdString();
	} else if (process.error() == QProcess::FailedToStart)
		return "; Failed to start " + processName;
	return "; Unknown error while running " + processName;
}

/**
 * @brief normalizeOBJCONV
 * @param text
 * @param bits
 * @return
 */
std::pair<QString, std::size_t /*insnLength*/> normalizeOBJCONV(const QString &text, int bits) {

	QRegExp expectedFormat("^ +([^;]+); ([0-9a-fA-F]+) _ (.*)");
	if (expectedFormat.indexIn(text, 0) == -1) {
		throw NormalizeFailure{};
	}

	const auto addr   = expectedFormat.cap(2).rightJustified(bits / 4, '0');
	auto bytes        = expectedFormat.cap(3).trimmed();
	const auto disasm = expectedFormat.cap(1).trimmed().replace(QRegExp("  +"), " ");
	const auto result = addr + "   " + bytes + "   " + disasm;

	bytes.replace(QRegExp("[^0-9a-fA-F]"), "");
	const std::size_t insnLength = bytes.length() / 2;
	return std::make_pair(result, insnLength);
}

/**
 * @brief runOBJCONV
 * @param bytes
 * @param address
 * @return
 */
std::string runOBJCONV(std::vector<std::uint8_t> bytes, edb::address_t address) {
	const std::string processName = "objconv";
	const auto bits               = edb::v1::debuggeeIs32Bit() ? 32 : 64;

	QString binaryFileName;
	{
		QTemporaryFile binary(QDir::tempPath() + "/edb_insn_inspector_temp_XXXXXX.bin");
		if (!binary.open()) {
			return "; Failed to create binary file";
		}

		{
#ifdef Q_OS_UNIX
			if (bits == 32) {
				struct FileData {
					Elf32_Ehdr elfHeader;
					Elf32_Phdr programHeader;
					Elf32_Shdr zerothSectionHeader = {};
					Elf32_Shdr codeSectionHeader;
					Elf32_Shdr stringTableSectionHeader;
					// start of zero fill + instruction
				} fileData;

				fileData.elfHeader = Elf32_Ehdr{
					{(unsigned char)ELFMAG0, ELFMAG1, ELFMAG2, ELFMAG3, ELFCLASS32, ELFDATA2LSB, EV_CURRENT},
					ET_EXEC,
					EM_386,
					EV_CURRENT,
					Elf32_Addr(address.toUint()),            // entry point
					offsetof(FileData, programHeader),       // program header table offset
					offsetof(FileData, zerothSectionHeader), // section header table offset
					0,                                       // flags
					sizeof(Elf32_Ehdr),                      // size of ELF header...
					sizeof(Elf32_Phdr),
					1, // number of program headers
					sizeof(Elf32_Shdr),
					3, // number of section headers (one for zeroth, another for ours,third string
					// table (which is only needed for objconv, not for correct ELF file))
					2 // string table index in section header table (could be SHN_UNDEF, but objconv is picky)
				};

				const edb::value32 insnAddr(address);

				// aligned on page boundary and one page before
				// Loading the file one page earlier to make it possible to have
				// code origin even at the beginning of actual page where ELF
				// header would otherwise be
				const auto pageSize         = 4096u;
				const edb::value32 fileAddr = (insnAddr & ~(pageSize - 1)) - pageSize;

				fileData.programHeader = Elf32_Phdr{
					PT_LOAD,
					0,                             // start of file is beginning of segment
					Elf32_Addr(fileAddr.toUint()), // vaddr of the segment
					0,                             // paddr of the segment, irrelevant
					2 * pageSize,                  // size of file image of the segment
					2 * pageSize,                  // size of memory image of the segment
					PF_R | PF_X,
					0 // no alignment requirements
				};

				fileData.codeSectionHeader = Elf32_Shdr{
					0, // index of name in string table
					SHT_PROGBITS,
					SHF_ALLOC | SHF_EXECINSTR,
					Elf32_Addr(address.toUint()),
					insnAddr - fileAddr, // section offset in file
					Elf32_Addr(bytes.size()),
					SHN_UNDEF, // sh_link
					0,         // sh_info
					0,         // alignment constraints: 0 or 1 mean unaligned
					0          // no fixed-size entries in this section
				};

				fileData.stringTableSectionHeader = Elf32_Shdr{
					0, // index of name in string table
					SHT_STRTAB,
					0,
					0,
					offsetof(FileData, zerothSectionHeader), // section offset in file: at the zeroth section header,
					// which starts with zeros
					1,         // single byte should be enough
					SHN_UNDEF, // sh_link
					0,         // sh_info
					0,         // alignment constraints: 0 or 1 mean unaligned
					0          // no fixed-size entries in this section
				};

				binary.write(reinterpret_cast<const char *>(&fileData), sizeof(fileData));
				binary.seek(insnAddr - fileAddr);
				binary.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
			} else if (bits == 64) {
				// FIXME: the file generated here is actually broken: I don't know why, but trying to run it will lead
				// to "Exec format error"
				struct FileData {
					Elf64_Ehdr elfHeader;
					Elf64_Phdr programHeader;
					Elf64_Shdr zerothSectionHeader = {};
					Elf64_Shdr codeSectionHeader;
					Elf64_Shdr stringTableSectionHeader;
					// start of zero fill + instruction
				} fileData;

				fileData.elfHeader = Elf64_Ehdr{
					{(unsigned char)ELFMAG0, ELFMAG1, ELFMAG2, ELFMAG3, ELFCLASS64, ELFDATA2LSB, EV_CURRENT},
					ET_EXEC,
					EM_386,
					EV_CURRENT,
					Elf64_Addr(address.toUint()),            // entry point
					offsetof(FileData, programHeader),       // program header table offset
					offsetof(FileData, zerothSectionHeader), // section header table offset
					0,                                       // flags
					sizeof(Elf64_Ehdr),                      // size of ELF header...
					sizeof(Elf64_Phdr),
					1, // number of program headers
					sizeof(Elf64_Shdr),
					3, // number of section headers (one for zeroth, another for ours,third string
					// table (which is only needed for objconv, not for correct ELF file))
					2 // string table index in section header table (could be SHN_UNDEF, but objconv is picky)
				};

				const edb::value64 insnAddr(address);
				// aligned on page boundary and one page before
				// Loading the file one page earlier to make it possible to have
				// code origin even at the beginning of actual page where ELF
				// header would otherwise be
				const auto pageSize         = 4096ull;
				const edb::value64 fileAddr = (insnAddr & ~(pageSize - 1)) - pageSize;

				fileData.programHeader = Elf64_Phdr{
					PT_LOAD,
					PF_R | PF_X,                   // NOTE: This field is placed differently than in Elf32_Phdr!!!
					0,                             // start of file is beginning of segment
					Elf64_Addr(fileAddr.toUint()), // vaddr of the segment
					0,                             // paddr of the segment, irrelevant
					2 * pageSize,                  // size of file image of the segment
					2 * pageSize,                  // size of memory image of the segment
					0                              // no alignment requirements
				};

				fileData.codeSectionHeader = Elf64_Shdr{
					0, // index of name in string table
					SHT_PROGBITS,
					SHF_ALLOC | SHF_EXECINSTR,
					Elf64_Addr(address.toUint()),
					insnAddr - fileAddr, // section offset in file
					bytes.size(),
					SHN_UNDEF, // sh_link
					0,         // sh_info
					0,         // alignment constraints: 0 or 1 mean unaligned
					0          // no fixed-size entries in this section
				};

				fileData.stringTableSectionHeader = Elf64_Shdr{
					0, // index of name in string table
					SHT_STRTAB,
					0,
					0,
					offsetof(FileData, zerothSectionHeader), // section offset in file: at the zeroth section header,
					// which starts with zeros
					1,         // single byte should be enough
					SHN_UNDEF, // sh_link
					0,         // sh_info
					0,         // alignment constraints: 0 or 1 mean unaligned
					0          // no fixed-size entries in this section
				};

				binary.write(reinterpret_cast<const char *>(&fileData), sizeof(fileData));
				binary.seek(insnAddr - fileAddr);
				binary.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
			} else {
				return "; " + std::to_string(bits) + " bit?.. Not implemented.";
			}
#endif
		}

		binary.close();
		binary.setPermissions(QFile::ExeOwner | QFile::ReadOwner | QFile::WriteOwner);
		// We have to destroy the QTemporaryFile object, otherwise for some reason we get
		// "Text file busy" when trying to run the binary. So will remove it manually.
		binary.setAutoRemove(false);
		binaryFileName = binary.fileName();
	}

	QProcess process;
	process.start(processName.c_str(), {"-fnasm", binaryFileName, "/dev/stdout"});

	const bool success = process.waitForFinished();
	QFile::remove(binaryFileName);

	if (success) {
		const auto output = process.readAllStandardOutput();
		const auto err    = process.readAllStandardError();

		if (process.exitCode() != 0) {
			return ("; got response: \"" + err + "\"").constData();
		}

		if (process.exitStatus() != QProcess::NormalExit) {
			return "; process crashed";
		}

		const auto lines = output.split('\n');
		QString result;
		enum class LookingFor { FunctionBegin,
								Instruction } mode = LookingFor::FunctionBegin;
		const auto addrFormatted                   = address.toHexString().toUpper().replace(QRegExp("^0+"), "");
		const auto addrTruncatedFormatted          = (address & UINT32_MAX).toHexString().toUpper().replace(QRegExp("^0+"), "");

		for (int L = 0; L < lines.size(); ++L) {
			const auto line = QString::fromUtf8(lines[L]);
			switch (mode) {
			case LookingFor::FunctionBegin:
				if (line.contains(QRegExp("^; Instruction set:"))) {
					result += line + '\n';
					continue;
				}

				if (line.contains(QRegExp("^SECTION.* execute"))) {
					mode = LookingFor::Instruction;
					continue;
				}
				break;
			case LookingFor::Instruction:
				if (line.startsWith("; ")) {
					// Filter useless notes
					if (line.contains("Function does not end with "))
						continue;
					if (line.contains("without relocation"))
						continue;

					result += line + '\n';
				}

				if (line.contains(QRegExp("  +[^;]+; 0*" + addrFormatted + " _")) || line.contains(QRegExp("  +[^;]+; 0*" + addrTruncatedFormatted + " _"))) { // XXX: objconv truncates all addresses to 32 bits
					try {
						QString normalized;
						std::size_t insnLength;
						std::tie(normalized, insnLength) = normalizeOBJCONV(line, bits);

						if (insnLength != bytes.size()) {
							// Avoid getting wrong "Instruction set: " print due to subsequent bytes possibly forming
							// some extended-ISA instruction
							bytes.resize(insnLength);
							return runOBJCONV(bytes, address);
						}

						result += normalized;
						return result.toStdString();
					} catch (NormalizeFailure &) {
						return "; !Failed to normalize\n" + QString::fromUtf8(output).toStdString();
					}
				}

				if (line.contains(QRegExp("^  +db "))) {
					auto lines = result.split('\n');
					for (int i = 0; i < result.size(); ++i)
						if (lines[i].startsWith("; Instruction set:")) {
							lines.removeAt(i);
							break;
						}
					return (lines.join("\n") + address.toHexString().toUpper() + "   " + line.trimmed()).toStdString();
				}
				break;
			}
		}

		return ("; Couldn't locate disassembly, stdout:\n\"" + output + "\"\nstderr:\n\"" + err + "\"").constData();
	} else if (process.error() == QProcess::FailedToStart) {
		return "; Failed to start " + processName;
	}

	return "; Unknown error while running " + processName;
}
#endif

}

class Disassembler {
private:
	csh csh_;
	cs_insn *insn_ = nullptr;

public:
	struct InitFailure {
		const char *error;
	};

	explicit Disassembler(cs_mode mode) {
		cs_err result = cs_open(
#if defined(EDB_X86) || defined(EDB_X86_64)
			CS_ARCH_X86
#elif defined(EDB_ARM32)
			CS_ARCH_ARM
#elif defined(EDB_ARM64)
			CS_ARCH_ARM64
#else
#error "What to pass to capstone?"
#endif
			,
			mode, &csh_);

		if (result != CS_ERR_OK) {
			throw InitFailure{cs_strerror(result)};
		}

		cs_option(csh_, CS_OPT_DETAIL, CS_OPT_ON);
		cs_option(csh_, CS_OPT_SYNTAX, edb::v1::config().syntax == Configuration::Syntax::Intel ? CS_OPT_SYNTAX_INTEL : CS_OPT_SYNTAX_ATT);
	}

	cs_insn *disassemble(const std::uint8_t *buf, std::size_t size, edb::address_t address) {
		if (insn_) {
			cs_free(insn_, 1);
		}

		if (cs_disasm(csh_, buf, size, address, 1, &insn_)) {
			return insn_;
		} else {
			return nullptr;
		}
	}

	~Disassembler() {
		if (insn_) {
			cs_free(insn_, 1);
		}

		cs_close(&csh_);
	}

	csh handle() const {
		return csh_;
	}
};

/**
 * @brief Plugin::Plugin
 * @param parent
 */
Plugin::Plugin(QObject *parent)
	: QObject(parent), menuAction_(new QAction("Inspect instruction (Capstone info)", this)) {

	connect(menuAction_, &QAction::triggered, this, [this](bool) {
		showDialog();
	});
}

/**
 * @brief Plugin::menu
 * @return
 */
QMenu *Plugin::menu(QWidget *) {
	return nullptr;
}

/**
 * @brief Plugin::cpuContextMenu
 * @return
 */
QList<QAction *> Plugin::cpuContextMenu() {
	return {menuAction_};
}

/**
 * @brief InstructionDialog::InstructionDialog
 * @param parent
 * @param f
 */
InstructionDialog::InstructionDialog(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {

	setWindowTitle("Instruction Inspector");
	address_ = edb::v1::cpu_selected_address();
	const cs_mode mode =
#if defined(EDB_X86) || defined(EDB_X86_64)
		edb::v1::debuggeeIs32Bit() ? CS_MODE_32 : CS_MODE_64
#elif defined(EDB_ARM32) || defined(EDB_ARM64)
		// FIXME(ARM): we also have possible values:
		//	* CS_MODE_ARM,
		//	* CS_MODE_THUMB,
		//	* CS_MODE_MCLASS,
		//	* CS_MODE_V8,
		//	and need to select the right one. Also need to choose from
		//	* CS_MODE_LITTLE_ENDIAN and
		//	* CS_MODE_BIG_ENDIAN
		static_cast<cs_mode>((edb::v1::debugger_core->cpuMode() == IDebugger::CpuMode::Thumb ? CS_MODE_THUMB : CS_MODE_ARM) | CS_MODE_LITTLE_ENDIAN)
#else
#error "What value should mode have?"
#endif
		;

	disassembler_ = new Disassembler(mode);

	uint8_t buffer[edb::Instruction::MaxSize];
	if (const int bufSize = edb::v1::get_instruction_bytes(address_, buffer)) {

		insnBytes_      = std::vector<std::uint8_t>(buffer, buffer + bufSize);
		const auto insn = disassembler_->disassemble(buffer, bufSize, address_);
		insn_           = insn;
		tree_           = new QTreeWidget;
		layout_         = new QVBoxLayout;

		setLayout(layout_);
		layout_->addWidget(tree_);
		buttonCompare_ = new QPushButton("Compare disassemblers");
		layout_->addWidget(buttonCompare_);

		connect(buttonCompare_, &QPushButton::clicked, this, [this](bool) {
			compareDisassemblers();
		});

		tree_->setUniformRowHeights(true);
		tree_->setColumnCount(2);
		tree_->setHeaderLabels({"Field", "Value"});

		// Workaround for impossibility of default parameters in C++11 lambdas
		struct Add {
			QTreeWidget *const tree_;
			explicit Add(QTreeWidget *tree)
				: tree_(tree) {
			}

			void operator()(const QStringList &sl, QTreeWidgetItem *parent = nullptr) const {
				tree_->addTopLevelItem(new QTreeWidgetItem(parent, sl));
			}
		} add(tree_);

		if (!insn) {
			add({"Bad instruction", "Failed to disassemble instruction at address " + edb::v1::format_pointer(address_)});
			add({"Bytes", printBytes(&insnBytes_[0], insnBytes_.size()).c_str()});
		} else {
			add({"Address", toHex(insn->address).c_str()});
			add({"Bytes", printBytes(insn->bytes, insn->size).c_str()});
			add({"Mnemonic", insn->mnemonic});
			add({"Operands string", insn->op_str});

			const auto groupNames = getGroupNames(disassembler_->handle(), insn);
			add({"Groups"});
			auto *const groups = tree_->topLevelItem(tree_->topLevelItemCount() - 1);

			for (const auto &group : groupNames) {
				add({group.c_str()}, groups);
			}

			{
				add({"Regs implicitly read"});
				auto *const regsRead = tree_->topLevelItem(tree_->topLevelItemCount() - 1);
				for (int r = 0; r < insn->detail->regs_read_count; ++r) {
					add({printReg(disassembler_->handle(), insn->detail->regs_read[r]).c_str()}, regsRead);
				}
			}

			{
				add({"Regs implicitly written"});
				auto *const regsWritten = tree_->topLevelItem(tree_->topLevelItemCount() - 1);
				for (int r = 0; r < insn->detail->regs_write_count; ++r) {
					add({printReg(disassembler_->handle(), insn->detail->regs_write[r]).c_str()}, regsWritten);
				}
			}
#if defined(EDB_X86) || defined(EDB_X86_64)
			add({"Prefixes", printBytes(insn->detail->x86.prefix, sizeof(insn->detail->x86.prefix), false).c_str()});
			add({"Opcode", printBytes(insn->detail->x86.opcode, sizeof(insn->detail->x86.opcode)).c_str()});

			if (insn->detail->x86.rex) {
				add({"REX", printBytes(&insn->detail->x86.rex, 1).c_str()});
			}

			add({"AddrSize", std::to_string(+insn->detail->x86.addr_size).c_str()});
			add({"ModRM", printBytes(&insn->detail->x86.modrm, 1).c_str()});
			add({"SIB", printBytes(&insn->detail->x86.sib, 1).c_str()});

			auto *const sib = tree_->topLevelItem(tree_->topLevelItemCount() - 1);
			add({"Displacement", toHex(insn->detail->x86.disp, true).c_str()}, sib);

			add({"index", printReg(disassembler_->handle(), insn->detail->x86.sib_index, true).c_str()}, sib);
			add({"scale", std::to_string(+insn->detail->x86.sib_scale).c_str()}, sib);
			add({"base", printReg(disassembler_->handle(), insn->detail->x86.sib_base, true).c_str()}, sib);

#if CS_API_MAJOR >= 4
			if (insn->detail->x86.xop_cc) {
				add({"XOP condition", printXOP_CC(insn->detail->x86.xop_cc).c_str()});
			}
#endif
			if (insn->detail->x86.sse_cc) {
				add({"SSE condition", printSSE_CC(insn->detail->x86.sse_cc).c_str()});
			}
			if (insn->detail->x86.avx_cc) {
				add({"AVX condition", printAVX_CC(insn->detail->x86.avx_cc).c_str()});
			}

			add({"SAE", insn->detail->x86.avx_sae ? "yes" : "no"});

			if (insn->detail->x86.avx_rm) {
				add({"AVX rounding", printAVX_RM(insn->detail->x86.avx_rm).c_str()});
			}

#if CS_API_MAJOR >= 4
			const auto changedEflagsNames = getChangedEFLAGSNames(insn->detail->x86.eflags);
			add({"EFLAGS"});
			auto *const eflags = tree_->topLevelItem(tree_->topLevelItemCount() - 1);
			for (auto efl : changedEflagsNames) {
				add({efl.c_str()}, eflags);
			}
#endif
			add({"Operands"});
			auto *const operands = tree_->topLevelItem(tree_->topLevelItemCount() - 1);
			for (int op = 0; op < insn->detail->x86.op_count; ++op) {

				const auto &operand = insn->detail->x86.operands[op];
				add({("#" + std::to_string(op + 1)).c_str()}, operands);

				auto *const curOpItem = operands->child(op);
				add({"Type", printOpType(operand.type).c_str()}, curOpItem);

				switch (operand.type) {
				case X86_OP_REG:
					add({"Register", printReg(disassembler_->handle(), operand.reg).c_str()}, curOpItem);
					break;
				case X86_OP_IMM:
					add({"Immediate", toHex(operand.imm).c_str()}, curOpItem);
					break;
				case X86_OP_MEM:
					add({"Segment", printReg(disassembler_->handle(), operand.mem.segment, true).c_str()}, curOpItem);
					add({"Base", printReg(disassembler_->handle(), operand.mem.base, true).c_str()}, curOpItem);
					add({"Index", printReg(disassembler_->handle(), operand.mem.index, true).c_str()}, curOpItem);
					add({"Scale", std::to_string(operand.mem.scale).c_str()}, curOpItem);
					add({"Displacement", toHex(operand.mem.disp, true).c_str()}, curOpItem);
					break;
				default:
					break;
				}

				add({"Size", (std::to_string(operand.size * 8) + " bit").c_str()}, curOpItem);
#if CS_API_MAJOR >= 4
				add({"Access", printAccessMode(operand.access).c_str()}, curOpItem);
#endif
				if (operand.avx_bcast) {
					add({"AVX Broadcast", printAVX_Bcast(operand.avx_bcast).c_str()}, curOpItem);
				}

				add({"AVX opmask", (operand.avx_zero_opmask ? "zeroing" : "merging")}, curOpItem);
			}
#elif defined(EDB_ARM32)
			add({"User mode regs", insn->detail->arm.usermode ? "True" : "False"});
			add({"Vector size", std::to_string(insn->detail->arm.vector_size).c_str()});
			// TODO: vector_data
			// TODO: cps_mode
			// TODO: cps_flag
			add({"Condition", printCond(insn->detail->arm.cc).c_str()});
			add({"Updates flags", insn->detail->arm.update_flags ? "True" : "False"});
			add({"Write-back", insn->detail->arm.writeback ? "True" : "False"});

			// TODO: mem_barrier
			add({"Operands"});

			auto *const operands = tree_->topLevelItem(tree_->topLevelItemCount() - 1);

			for (int op = 0; op < insn->detail->arm.op_count; ++op) {
				const auto &operand = insn->detail->arm.operands[op];
				add({("#" + std::to_string(op + 1)).c_str()}, operands);
				auto *const curOpItem = operands->child(op);

				if (operand.vector_index != -1) {
					add({"Vector index", std::to_string(operand.vector_index).c_str()}, curOpItem);
				}

				if (operand.shift.type) {
					add({"Shift type", printShiftType(operand.shift.type).c_str()}, curOpItem);
					add({"Shift", std::to_string(operand.shift.value).c_str()}, curOpItem);
				}

				add({"Type", printOpType(operand.type).c_str()}, curOpItem);

				switch (operand.type) {
				case ARM_OP_SYSREG:
				case ARM_OP_REG:
					add({"Register", printReg(disassembler_->handle(), operand.reg).c_str()}, curOpItem);
					break;
				case ARM_OP_CIMM:
				case ARM_OP_PIMM:
				case ARM_OP_IMM:
					add({"Immediate", toHex(util::to_unsigned(operand.imm)).c_str()}, curOpItem);
					break;
				case ARM_OP_FP:
					add({"Float", toFloatString(operand.fp).c_str()}, curOpItem);
					break;
				case ARM_OP_MEM:
					add({"Base", printReg(disassembler_->handle(), operand.mem.base, true).c_str()}, curOpItem);
					add({"Index", printReg(disassembler_->handle(), operand.mem.index, true).c_str()}, curOpItem);
					add({"Scale", std::to_string(operand.mem.scale).c_str()}, curOpItem);
					add({"Displacement", toHex(operand.mem.disp, true).c_str()}, curOpItem);
#if CS_API_MAJOR >= 4
					add({"Left shift", std::to_string(operand.mem.lshift).c_str()}, curOpItem);
#endif
					break;
				case ARM_OP_SETEND:
					add({"Type", printOpType(operand.setend).c_str()}, curOpItem);
					break;
				}

				add({"Subtracted", operand.subtracted ? "True" : "False"}, curOpItem);
#if CS_API_MAJOR >= 4
				add({"Access", printAccessMode(operand.access).c_str()}, curOpItem);
				if (operand.neon_lane != -1) {
					add({"NEON lane", std::to_string(operand.neon_lane).c_str()}, curOpItem);
				}
#endif
			}
#endif
		}

		tree_->expandAll();
		tree_->resizeColumnToContents(0);
	} else {
		QMessageBox::critical(
			edb::v1::debugger_ui,
			tr("Error reading instruction"),
			tr("Failed to read instruction at address %1").arg(edb::v1::format_pointer(address_)));
		throw InstructionReadFailure{};
	}
}

/**
 * @brief InstructionDialog::compareDisassemblers
 */
void InstructionDialog::compareDisassemblers() {

	std::ostringstream message;
	message << "capstone:\n";

	if (const auto insn = static_cast<cs_insn *>(insn_)) {
		message << address_.toHexString().toUpper().toStdString()
				<< "   " << printBytes(insn->bytes, insn->size)
				<< "   " << insn->mnemonic << " " << insn->op_str;
	} else {
		message << address_.toHexString().toUpper().toStdString()
				<< "   " << printBytes(insnBytes_.data(), 1)
				<< "   db " << toHex(insnBytes_[0]);
	}

#if defined(EDB_X86) || defined(EDB_X86_64)
	message << "\n\n";
	message << "ndisasm:\n";
	message << runNDISASM(insnBytes_, address_);
#endif
	message << "\n\n";
	message << "objdump:\n";
	message << runOBJDUMP(insnBytes_, address_);
#if defined(EDB_X86) || defined(EDB_X86_64)
	message << "\n\n";
	message << "objconv:\n";
	message << runOBJCONV(insnBytes_, address_);
#endif

	buttonCompare_->deleteLater();

	constexpr int initialTextBrowserSize = 200;
	auto splitter                        = new QSplitter(this);
	splitter->setOrientation(Qt::Vertical);
	splitter->addWidget(tree_);
	splitter->addWidget(disassemblies_ = new QTextBrowser);
	splitter->setSizes({height() - initialTextBrowserSize, initialTextBrowserSize});
	layout_->addWidget(splitter);

	QFont font(disassemblies_->font());
	font.setStyleHint(QFont::TypeWriter);
	font.setFamily("Monospace");

	disassemblies_->setFont(font);
	disassemblies_->setText(message.str().c_str());
}

/**
 * @brief InstructionDialog::~InstructionDialog
 */
InstructionDialog::~InstructionDialog() {
	delete disassembler_;
}

/**
 * @brief Plugin::showDialog
 */
void Plugin::showDialog() const {
	try {
		const auto dialog = new InstructionDialog(edb::v1::debugger_ui);
		dialog->setAttribute(Qt::WA_DeleteOnClose);
		dialog->resize(800, 500); // TODO: make it depend on size hints
		dialog->show();
	} catch (Disassembler::InitFailure &ex) {
		QMessageBox::critical(
			edb::v1::debugger_ui,
			tr("Capstone error"),
			tr("Failed to initialize Capstone: %1").arg(ex.error));
	} catch (...) {
	}
}

}
