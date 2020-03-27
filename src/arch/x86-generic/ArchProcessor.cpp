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

#include "ArchProcessor.h"
#include "Configuration.h"
#include "FloatX.h"
#include "IDebugger.h"
#include "IProcess.h"
#include "IThread.h"
#include "Instruction.h"
#include "Prototype.h"
#include "RegisterViewModel.h"
#include "State.h"
#include "Util.h"
#include "edb.h"
#include "string_hash.h"

#include <QApplication>
#include <QDebug>
#include <QDomDocument>
#include <QFile>
#include <QVector>
#include <QWidget>
#include <QXmlQuery>

#include <cctype>
#include <climits>
#include <cmath>
#include <cstring>
#include <memory>

#ifdef Q_OS_LINUX
#include "errno-names-linux.h"
#include <asm/unistd.h>
#endif

namespace ILP32 {

constexpr std::int32_t toInt(std::uint64_t x) {
	return x;
}
constexpr std::uint32_t toUInt(std::uint64_t x) {
	return x;
}
constexpr std::int32_t toLong(std::uint64_t x) {
	return x;
}
constexpr std::uint32_t toULong(std::uint64_t x) {
	return x;
}

}

namespace LP64 {

constexpr std::int32_t toInt(std::uint64_t x) {
	return x;
}
constexpr std::uint32_t toUInt(std::uint64_t x) {
	return x;
}
constexpr std::int64_t toLong(std::uint64_t x) {
	return x;
}
constexpr std::uint64_t toULong(std::uint64_t x) {
	return x;
}

}

namespace {

using std::size_t;

enum RegisterIndex {
	rAX = 0,
	rCX = 1,
	rDX = 2,
	rBX = 3,
	rSP = 4,
	rBP = 5,
	rSI = 6,
	rDI = 7,
	R8  = 8,
	R9  = 9,
	R10 = 10,
	R11 = 11,
	R12 = 12,
	R13 = 13,
	R14 = 14,
	R15 = 15,
};

enum SegmentRegisterIndex {
	ES,
	CS,
	SS,
	DS,
	FS,
	GS,
};

constexpr size_t MAX_DEBUG_REGS_COUNT = 8;
constexpr size_t GPR32_COUNT          = 8;
constexpr size_t GPR64_COUNT          = 16;
constexpr size_t SSE32_COUNT          = GPR32_COUNT;
constexpr size_t SSE64_COUNT          = GPR64_COUNT;
constexpr size_t AVX32_COUNT          = SSE32_COUNT;
constexpr size_t AVX64_COUNT          = SSE64_COUNT;
constexpr size_t MAX_FPU_REGS_COUNT   = 8;
constexpr size_t MAX_MMX_REGS_COUNT   = MAX_FPU_REGS_COUNT;

using edb::v1::debuggeeIs32Bit;
using edb::v1::debuggeeIs64Bit;

int func_param_regs_count() {
	return debuggeeIs32Bit() ? 0 : 6;
}

using MMWord  = edb::value64;
using XMMWord = edb::value128;
using YMMWord = edb::value256;
using ZMMWord = edb::value512;

template <typename T>
std::string register_name(const T &val) {
	return edb::v1::formatter().registerName(val);
}

template <typename T>
QString syscallErrName(T err) {
#ifdef Q_OS_LINUX
	std::size_t index = -err;

	if (index >= errnoNames.size()) {
		return "";
	}

	if (errnoNames[index]) {
		return errnoNames[index];
	}
#else
	Q_UNUSED(err)
#endif
	return "";
}

//------------------------------------------------------------------------------
// Name: format_pointer
// Desc:
//------------------------------------------------------------------------------
QString format_pointer(int pointer_level, edb::reg_t arg, QChar type) {

	Q_UNUSED(type)
	Q_UNUSED(pointer_level)

	if (arg == 0) {
		return "NULL";
	} else {
		return edb::v1::format_pointer(arg);
	}
}

//------------------------------------------------------------------------------
// Name: format_integer
// Desc:
//------------------------------------------------------------------------------
QString format_integer(int pointer_level, edb::reg_t arg, QChar type) {
	if (pointer_level > 0) {
		return format_pointer(pointer_level, arg, type);
	}

	switch (type.toLatin1()) {
	case 'w':
		return "0x" + QString::number(static_cast<wchar_t>(arg), 16);
	case 'b':
		return arg ? "true" : "false";
	case 'c':
		if (arg < 0x80u && (std::isprint(arg) || std::isspace(arg))) {
			return QString("'%1'").arg(static_cast<char>(arg));
		} else {
			return QString("'\\x%1'").arg(static_cast<uint16_t>(arg), 2, 16);
		}
	case 'a':
		// signed char; since we're formatting as hex, we want to avoid sign
		// extension done inside QString::number (happening due to the cast to
		// qlonglong inside QString::setNum, which used in QString::number).
		// Similarly for other shorter-than-long-long signed types.
	case 'h':
		return "0x" + QString::number(static_cast<unsigned char>(arg), 16);
	case 's':
	case 't':
		return "0x" + QString::number(static_cast<unsigned short>(arg), 16);
	case 'i':
	case 'j':
		return "0x" + QString::number(debuggeeIs32Bit() ? ILP32::toUInt(arg) : LP64::toUInt(arg), 16);
	case 'l':
	case 'm':
		return "0x" + QString::number(debuggeeIs32Bit() ? ILP32::toULong(arg) : LP64::toULong(arg), 16);
	case 'x':
		return "0x" + QString::number(static_cast<long long>(arg), 16);
	case 'y':
		return "0x" + QString::number(static_cast<long unsigned long>(arg), 16);
	case 'n':
	case 'o':
	default:
		return format_pointer(pointer_level, arg, type);
	}
}

//------------------------------------------------------------------------------
// Name: format_integer
// Desc:
//------------------------------------------------------------------------------
QString format_char(int pointer_level, edb::address_t arg, QChar type) {

	if (IProcess *process = edb::v1::debugger_core->process()) {
		if (pointer_level == 1) {
			if (arg == 0) {
				return "NULL";
			} else {
				QString string_param;
				int string_length;

				if (edb::v1::get_ascii_string_at_address(arg, string_param, edb::v1::config().min_string_length, 256, string_length)) {
					return QString("<%1> \"%2\"").arg(edb::v1::format_pointer(arg), string_param);
				} else {
					char character;
					process->readBytes(arg, &character, sizeof(character));
					if (character == '\0') {
						return QString("<%1> \"\"").arg(edb::v1::format_pointer(arg));
					} else {
						return QString("<%1>").arg(edb::v1::format_pointer(arg));
					}
				}
			}
		} else {
			return format_integer(pointer_level, arg, type);
		}
	}

	return "?";
}

//------------------------------------------------------------------------------
// Name: format_argument
// Desc:
//------------------------------------------------------------------------------
QString format_argument(const QString &type, const Register &arg) {

	if (!arg) return QObject::tr("(failed to get value)");
	int pointer_level = 0;
	for (QChar ch : type) {

		if (ch == 'P') {
			++pointer_level;
		} else if (ch == 'r' || ch == 'V' || ch == 'K') {
			// skip things like const, volatile, restrict, they don't effect
			// display for us
			continue;
		} else {
			switch (ch.toLatin1()) {
			case 'v':
				return format_pointer(pointer_level, arg.valueAsAddress(), ch);
			case 'w':
				return format_integer(pointer_level, arg.valueAsInteger(), ch);
			case 'b':
				return format_integer(pointer_level, arg.valueAsSignedInteger(), ch);
			case 'c':
				return format_char(pointer_level, arg.valueAsAddress(), ch);
			case 'a':
				return format_integer(pointer_level, arg.valueAsSignedInteger(), ch);
			case 'h':
				return format_integer(pointer_level, arg.valueAsInteger(), ch);
			case 's':
				return format_integer(pointer_level, arg.valueAsSignedInteger(), ch);
			case 't':
				return format_integer(pointer_level, arg.valueAsInteger(), ch);
			case 'i':
				return format_integer(pointer_level, arg.valueAsSignedInteger(), ch);
			case 'j':
				return format_integer(pointer_level, arg.valueAsInteger(), ch);
			case 'l':
				return format_integer(pointer_level, arg.valueAsSignedInteger(), ch);
			case 'm':
				return format_integer(pointer_level, arg.valueAsInteger(), ch);
			case 'x':
				return format_integer(pointer_level, arg.valueAsSignedInteger(), ch);
			case 'y':
				return format_integer(pointer_level, arg.valueAsInteger(), ch);
			case 'n':
				return format_integer(pointer_level, arg.valueAsSignedInteger(), ch);
			case 'o':
				return format_integer(pointer_level, arg.valueAsSignedInteger(), ch);
			case 'f':
			case 'd':
			case 'e':
			case 'g':
			case 'z':
			default:
				break;
			}
		}
	}

	return format_pointer(pointer_level, arg.valueAsAddress(), 'x');
}

template <class T>
void resolve_function_parameters_helper(T parameter_registers, const State &state, const QString &symname, int offset, QStringList &ret) {
	static const QString prefix(QLatin1String("!"));

	if (IProcess *process = edb::v1::debugger_core->process()) {
		// we will always be removing the last 2 chars '+0' from the string as well
		// as chopping the region prefix we like to prepend to symbols
		QString func_name;
		const int colon_index = symname.indexOf(prefix);

		if (colon_index != -1) {
			func_name = symname.left(symname.length() - 2).mid(colon_index + prefix.size());
		}

		// safe not to check for -1, it means 'rest of string' for the mid function
		func_name = func_name.mid(0, func_name.indexOf("@"));

		if (const edb::Prototype *const info = edb::v1::get_function_info(func_name)) {

			QStringList arguments;
			int i = 0;
			for (const edb::Argument &argument : info->arguments) {

				Register arg;
				if (i + 1 > func_param_regs_count()) {
					size_t arg_i_position = (i - func_param_regs_count()) * edb::v1::pointer_size();
					edb::reg_t value(0);
					process->readBytes(state.stackPointer() + offset + arg_i_position, &value, edb::v1::pointer_size());
					arg = edb::v1::debuggeeIs64Bit() ? make_Register<64>("", value, Register::TYPE_GPR) : make_Register<32>("", value, Register::TYPE_GPR);
				} else {
					arg = state[parameter_registers[i]];
				}

				arguments << format_argument(argument.type, arg);
				++i;
			}

			ret << QString("%1(%2)").arg(func_name, arguments.join(", "));
		}
	}
}

//------------------------------------------------------------------------------
// Name: resolve_function_parameters
// Desc:
//------------------------------------------------------------------------------
void resolve_function_parameters(const State &state, const QString &symname, int offset, QStringList &ret) {

	/*
	 * The calling convention of the AMD64 application binary interface is
	 * followed on Linux and other non-Microsoft operating systems.
	 * The registers RDI, RSI, RDX, RCX, R8 and R9 are used for integer and
	 * pointer arguments while XMM0, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6 and
	 * XMM7 are used for floating point arguments. As in the Microsoft x64
	 * calling convention, additional arguments are pushed onto the stack and
	 * the return value is stored in RAX.
	 */
	static const std::array<const char *, 6> parameter_registers_x64 = {
		"rdi",
		"rsi",
		"rdx",
		"rcx",
		"r8",
		"r9"};

	static const std::array<const char *, 0> parameter_registers_x86 = {};

	if (debuggeeIs64Bit()) {
		resolve_function_parameters_helper(parameter_registers_x64, state, symname, offset, ret);
	} else {
		resolve_function_parameters_helper(parameter_registers_x86, state, symname, offset, ret);
	}
}

//------------------------------------------------------------------------------
// Name: is_jcc_taken
// Desc:
//------------------------------------------------------------------------------
bool is_jcc_taken(const edb::reg_t efl, edb::Instruction::ConditionCode cond) {

	const bool cf = (efl & 0x0001) != 0;
	const bool pf = (efl & 0x0004) != 0;
	const bool zf = (efl & 0x0040) != 0;
	const bool sf = (efl & 0x0080) != 0;
	const bool of = (efl & 0x0800) != 0;

	bool taken = false;

	switch (cond & 0x0e) {
	case 0x00:
		taken = of;
		break;
	case 0x02:
		taken = cf;
		break;
	case 0x04:
		taken = zf;
		break;
	case 0x06:
		taken = cf || zf;
		break;
	case 0x08:
		taken = sf;
		break;
	case 0x0a:
		taken = pf;
		break;
	case 0x0c:
		taken = sf != of;
		break;
	case 0x0e:
		taken = zf || sf != of;
		break;
	}

	if (cond & 0x01) {
		taken = !taken;
	}

	return taken;
}

//------------------------------------------------------------------------------
// Name: is_jcc_taken
// Desc:
//------------------------------------------------------------------------------
bool is_jcc_taken(const State &state, edb::Instruction::ConditionCode cond) {

	if (cond == edb::Instruction::CC_UNCONDITIONAL) return true;
	if (cond == edb::Instruction::CC_RCXZ) return state.gpRegister(rCX).value<edb::value64>() == 0;
	if (cond == edb::Instruction::CC_ECXZ) return state.gpRegister(rCX).value<edb::value32>() == 0;
	if (cond == edb::Instruction::CC_CXZ) return state.gpRegister(rCX).value<edb::value16>() == 0;

	return is_jcc_taken(state.flags(), cond);
}

static const QLatin1String jumpConditionMnemonics[] = {
	QLatin1String("O"), QLatin1String("NO"),
	QLatin1String("B"), QLatin1String("AE"),
	QLatin1String("E"), QLatin1String("NE"),
	QLatin1String("BE"), QLatin1String("A"),
	QLatin1String("S"), QLatin1String("NS"),
	QLatin1String("P"), QLatin1String("NP"),
	QLatin1String("L"), QLatin1String("GE"),
	QLatin1String("LE"), QLatin1String("G")};

//------------------------------------------------------------------------------
// Name: analyze_cmov
// Desc:
//------------------------------------------------------------------------------
void analyze_cmov(const State &state, const edb::Instruction &inst, QStringList &ret) {

	const bool taken = is_jcc_taken(state, inst.conditionCode());

	if (taken) {
		ret << ArchProcessor::tr("move performed");
	} else {
		ret << ArchProcessor::tr("move NOT performed");
	}
}

//------------------------------------------------------------------------------
// Name: analyze_jump
// Desc:
//------------------------------------------------------------------------------
void analyze_jump(const State &state, const edb::Instruction &inst, QStringList &ret) {

	bool taken = false;

	if (is_conditional_jump(inst)) {
		taken = is_jcc_taken(state, inst.conditionCode());
	}

	if (taken) {
		ret << ArchProcessor::tr("jump taken");
	} else {
		ret << ArchProcessor::tr("jump NOT taken");
	}
}

//------------------------------------------------------------------------------
// Name: analyze_return
// Desc:
//------------------------------------------------------------------------------
void analyze_return(const State &state, const edb::Instruction &inst, QStringList &ret) {

	Q_UNUSED(inst)

	if (IProcess *process = edb::v1::debugger_core->process()) {
		edb::address_t return_address(0);
		process->readBytes(state.stackPointer(), &return_address, edb::v1::pointer_size());

		const QString symname = edb::v1::find_function_symbol(return_address);
		if (!symname.isEmpty()) {
			ret << ArchProcessor::tr("return to %1 <%2>").arg(edb::v1::format_pointer(return_address), symname);
		} else {
			ret << ArchProcessor::tr("return to %1").arg(edb::v1::format_pointer(return_address));
		}
	}
}

//------------------------------------------------------------------------------
// Name: analyze_call
// Desc:
//------------------------------------------------------------------------------
void analyze_call(const State &state, const edb::Instruction &inst, QStringList &ret) {

	if (IProcess *process = edb::v1::debugger_core->process()) {

		if (const auto operand = inst[0]) {

			bool ok;
			const edb::address_t effective_address = edb::v1::arch_processor().getEffectiveAddress(inst, operand, state, ok);
			if (!ok) return;
			const auto temp_operand = QString::fromStdString(edb::v1::formatter().toString(operand));

			if (is_immediate(operand)) {
				int offset;
				const QString symname = edb::v1::find_function_symbol(effective_address, QString(), &offset);

				if (!symname.isEmpty()) {
					ret << QString("%1 = %2 <%3>").arg(temp_operand, edb::v1::format_pointer(effective_address), symname);

					if (offset == 0) {
						if (is_call(inst)) {
							resolve_function_parameters(state, symname, 0, ret);
						} else {
							resolve_function_parameters(state, symname, 4, ret);
						}
					}

				} else {
#if 0
					ret << QString("%1 = %2").arg(temp_operand, edb::v1::format_pointer(effective_address));
#endif
				}
			} else if (is_register(operand)) {
				int offset;
				const QString symname = edb::v1::find_function_symbol(effective_address, QString(), &offset);
				if (!symname.isEmpty()) {
					ret << QString("%1 = %2 <%3>").arg(temp_operand, edb::v1::format_pointer(effective_address), symname);

					if (offset == 0) {
						if (is_call(inst)) {
							resolve_function_parameters(state, symname, 0, ret);
						} else {
							resolve_function_parameters(state, symname, 4, ret);
						}
					}

				} else {
					ret << QString("%1 = %2").arg(temp_operand, edb::v1::format_pointer(effective_address));
				}
			} else if (is_expression(operand)) {
				edb::address_t target(0);

				if (process->readBytes(effective_address, &target, edb::v1::pointer_size())) {
					int offset;
					const QString symname = edb::v1::find_function_symbol(target, QString(), &offset);
					if (!symname.isEmpty()) {
						ret << QString("%1 = [%2] = %3 <%4>").arg(temp_operand, edb::v1::format_pointer(effective_address), edb::v1::format_pointer(target), symname);

						if (offset == 0) {
							if (is_call(inst)) {
								resolve_function_parameters(state, symname, 0, ret);
							} else {
								resolve_function_parameters(state, symname, 4, ret);
							}
						}

					} else {
						ret << QString("%1 = [%2] = %3").arg(temp_operand, edb::v1::format_pointer(effective_address), edb::v1::format_pointer(target));
					}
				} else {
					// could not read from the address
					ret << QString("%1 = [%2] = ?").arg(temp_operand, edb::v1::format_pointer(effective_address));
				}
			}
		}
	}
}

bool isFPU_BCD(const edb::Instruction &inst) {

	const auto op = inst.operation();
	return op == X86_INS_FBLD ||
		   op == X86_INS_FBSTP;
}

QString formatBCD(const edb::value80 &v) {

	auto hex = v.toHexString();
	// Low bytes which contain 18 digits must be decimal. If not, return the raw hex value.
	if (hex.mid(2).contains(QRegExp("[A-Fa-f]")))
		return "0x" + hex;
	hex.replace(QRegExp("^..0*"), "");
	return (v.negative() ? '-' + hex : hex) + " (BCD)";
}

template <typename ValueType>
QString formatPackedFloat(const char *data, std::size_t size) {

	QString str;
	for (std::size_t offset = 0; offset < size; offset += sizeof(ValueType)) {

		ValueType value;
		std::memcpy(&value, data + offset, sizeof(value));
		if (!str.isEmpty()) str += ", ";
		str += format_float(value);
	}
	return size == sizeof(ValueType) ? str : '{' + str + '}';
}

//------------------------------------------------------------------------------
// Name: analyze_operands
// Desc:
//------------------------------------------------------------------------------
void analyze_operands(const State &state, const edb::Instruction &inst, QStringList &ret) {

	Q_UNUSED(inst)

	if (IProcess *process = edb::v1::debugger_core->process()) {

		for (std::size_t j = 0; j < inst.operandCount(); ++j) {

			const auto operand = inst[j];

			if (operand) {

				QString temp_operand = QString::fromStdString(edb::v1::formatter().toString(operand));

				if (is_immediate(operand)) {
#if 0
					bool ok;
					const edb::address_t effective_address = edb::v1::arch_processor().getEffectiveAddress(inst, operand, state, ok);
					if (!ok) return;
					ret << QString("%1 = %2").arg(temp_operand).arg(edb::v1::format_pointer(effective_address));
#endif
				} else if (is_register(operand)) {
					Register reg = state[QString::fromStdString(edb::v1::formatter().toString(operand))];
					QString valueString;
					if (!reg)
						valueString = ArchProcessor::tr("(Error: obtained invalid register value from State)");
					else {
						if (reg.type() == Register::TYPE_FPU && reg.bitSize() == 80)
							valueString = format_float(reg.value<edb::value80>());
						else if (is_SIMD_SS(operand)) {
							valueString = format_float(reg.value<edb::value32>());
							temp_operand += "_ss";
						} else if (is_SIMD_SD(operand)) {
							valueString = format_float(reg.value<edb::value64>());
							temp_operand += "_sd";
						} else if (is_SIMD_PS(operand))
							valueString = formatPackedFloat<edb::value32>(reg.rawData(), reg.bitSize() / 8);
						else if (is_SIMD_PD(operand))
							valueString = formatPackedFloat<edb::value64>(reg.rawData(), reg.bitSize() / 8);
						else {
							const bool simdSI  = is_SIMD_SI(operand);
							const bool simdUSI = !simdSI && is_SIMD_USI(operand);
							if ((simdSI || simdUSI) && (reg.bitSize() == 32 || reg.bitSize() == 64)) {
								const auto mode = simdUSI ? NumberDisplayMode::Unsigned : NumberDisplayMode::Signed;
								std::int64_t signedValue;
								if (reg.bitSize() == 32) {
									signedValue = reg.value<edb::value32>();
									valueString = util::format_int(reg.value<edb::value32>(), mode);
								} else {
									signedValue = reg.value<edb::value64>();
									valueString = util::format_int(reg.value<edb::value64>(), mode);
								}
								// FIXME: we have to explicitly say it's decimal because EDB is pretty inconsistent
								// even across values in analysis view about its use of 0x prefix
								// Use of hexadecimal format here is pretty much pointless since the number here is
								// expected to be used in usual numeric computations, not as address or similar
								if (signedValue > 9 || signedValue < -9)
									valueString += " (decimal)";
							} else
								valueString = "0x" + reg.toHexString();
						}
					}
					ret << QString("%1 = %2").arg(temp_operand, valueString);
				} else if (is_expression(operand)) {
					bool ok;
					const edb::address_t effective_address = edb::v1::arch_processor().getEffectiveAddress(inst, operand, state, ok);
					if (!ok) continue;
					edb::value256 target;

					if (process->readBytes(effective_address, &target, sizeof(target))) {

						switch (operand->size) {
						case 1:
							ret << QString("%1 = [%2] = 0x%3").arg(temp_operand, edb::v1::format_pointer(effective_address), edb::value8(target).toHexString());
							break;
						case 2: {
							const edb::value16 value(target);
							QString valueStr;
							if (is_fpu_taking_integer(inst)) {
								valueStr = util::format_int(value, NumberDisplayMode::Signed);
								// FIXME: we have to explicitly say it's decimal because EDB is pretty inconsistent
								// even across values in analysis view about its use of 0x prefix
								// Use of hexadecimal format here is pretty much pointless since the number here is
								// expected to be used in usual numeric computations, not as address or similar
								const std::int16_t signedValue = value;
								if (signedValue > 9 || signedValue < -9)
									valueStr += " (decimal)";
							} else
								valueStr = "0x" + value.toHexString();
							ret << QString("%1 = [%2] = %3").arg(temp_operand, edb::v1::format_pointer(effective_address), valueStr);
							break;
						}
						case 4: {
							const edb::value32 value(target);
							const bool simdSI  = is_SIMD_SI(operand);
							const bool simdUSI = !simdSI && is_SIMD_USI(operand);
							QString valueStr;
							if (is_fpu_taking_float(inst) || is_SIMD_SS(operand))
								valueStr = format_float(value);
							else if (is_fpu_taking_integer(inst) || simdSI || simdUSI) {
								valueStr = util::format_int(value, simdUSI ? NumberDisplayMode::Unsigned : NumberDisplayMode::Signed);
								// FIXME: we have to explicitly say it's decimal because EDB is pretty inconsistent
								// even across values in analysis view about its use of 0x prefix
								// Use of hexadecimal format here is pretty much pointless since the number here is
								// expected to be used in usual numeric computations, not as address or similar
								const std::int32_t signedValue = value;
								if (signedValue > 9 || signedValue < -9)
									valueStr += " (decimal)";
							} else if (is_SIMD_PS(operand))
								valueStr = formatPackedFloat<edb::value32>(reinterpret_cast<const char *>(&target), sizeof(edb::value64));
							else if (is_SIMD_PD(operand))
								valueStr = formatPackedFloat<edb::value64>(reinterpret_cast<const char *>(&target), sizeof(edb::value64));
							else
								valueStr = "0x" + value.toHexString();
							ret << QString("%1 = [%2] = %3").arg(temp_operand, edb::v1::format_pointer(effective_address), valueStr);
							break;
						}
						case 8: {
							const edb::value64 value(target);
							const bool simdSI  = is_SIMD_SI(operand);
							const bool simdUSI = !simdSI && is_SIMD_USI(operand);
							QString valueStr;
							if (is_fpu_taking_float(inst) || is_SIMD_SD(operand))
								valueStr = format_float(value);
							else if (is_fpu_taking_integer(inst) || simdSI || simdUSI) {
								valueStr = util::format_int(value, simdUSI ? NumberDisplayMode::Unsigned : NumberDisplayMode::Signed);
								// FIXME: we have to explicitly say it's decimal because EDB is pretty inconsistent
								// even across values in analysis view about its use of 0x prefix
								// Use of hexadecimal format here is pretty much pointless since the number here is
								// expected to be used in usual numeric computations, not as address or similar
								const std::int64_t signedValue = value;
								if (signedValue > 9 || signedValue < -9)
									valueStr += " (decimal)";
							} else
								valueStr = "0x" + value.toHexString();
							ret << QString("%1 = [%2] = %3").arg(temp_operand, edb::v1::format_pointer(effective_address), valueStr);
							break;
						}
						case 10: {
							const edb::value80 value(target);
							const QString valueStr = is_fpu(inst) ? isFPU_BCD(inst) ? formatBCD(value) : format_float(value) : "0x" + value.toHexString();
							ret << QString("%1 = [%2] = %3").arg(temp_operand, edb::v1::format_pointer(effective_address), valueStr);
							break;
						}
						case 16: {
							QString valueString;
							if (is_SIMD_PS(operand))
								valueString = formatPackedFloat<edb::value32>(reinterpret_cast<const char *>(&target), sizeof(edb::value128));
							else if (is_SIMD_PD(operand))
								valueString = formatPackedFloat<edb::value64>(reinterpret_cast<const char *>(&target), sizeof(edb::value128));
							else
								valueString = "0x" + edb::value128(target).toHexString();
							ret << QString("%1 = [%2] = %3").arg(temp_operand, edb::v1::format_pointer(effective_address), valueString);
							break;
						}
						case 32: {
							QString valueString;
							if (is_SIMD_PS(operand))
								valueString = formatPackedFloat<edb::value32>(reinterpret_cast<const char *>(&target), sizeof(edb::value256));
							else if (is_SIMD_PD(operand))
								valueString = formatPackedFloat<edb::value64>(reinterpret_cast<const char *>(&target), sizeof(edb::value256));
							else
								valueString = "0x" + edb::value256(target).toHexString();
							ret << QString("%1 = [%2] = %3").arg(temp_operand, edb::v1::format_pointer(effective_address), valueString);
							break;
						}
						default:
							ret << QString("%1 = [%2] = 0x%3").arg(temp_operand, edb::v1::format_pointer(effective_address), QString("<Error: unexpected size; low bytes form %2>").arg(target.toHexString()));
							break;
						}
					} else {
						ret << QString("%1 = [%2] = ?").arg(temp_operand, edb::v1::format_pointer(effective_address));
					}
				}
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: analyze_jump_targets
// Desc:
//------------------------------------------------------------------------------
void analyze_jump_targets(const edb::Instruction &inst, QStringList &ret) {
	const edb::address_t address       = inst.rva();
	const edb::address_t start_address = address - 128;
	const edb::address_t end_address   = address + 127;

	uint8_t buffer[edb::Instruction::MaxSize];

	for (edb::address_t addr = start_address; addr < end_address; ++addr) {
		if (const int sz = edb::v1::get_instruction_bytes(addr, buffer)) {
			edb::Instruction inst(buffer, buffer + sz, addr);
			if (is_jump(inst)) {
				const auto operand = inst[0];

				if (is_immediate(operand)) {
					const edb::address_t target = operand->imm;

					if (target == address) {
						ret << ArchProcessor::tr("possible jump from %1").arg(edb::v1::format_pointer(addr));
					}
				}
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: analyze_syscall
// Desc:
//------------------------------------------------------------------------------
void analyze_syscall(const State &state, const edb::Instruction &inst, QStringList &ret, std::uint64_t regAX) {
	Q_UNUSED(inst)
	Q_UNUSED(ret)
	Q_UNUSED(state)

#ifdef Q_OS_LINUX
	const bool isX32 = regAX & __X32_SYSCALL_BIT;
	regAX &= ~__X32_SYSCALL_BIT;

	QString syscall_entry;
	QDomDocument syscall_xml;
	QFile file(":/debugger/xml/syscalls.xml");
	if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {

		QXmlQuery query;
		query.setFocus(&file);
		const QString arch = debuggeeIs64Bit() ? "x86-64" : "x86";
		query.setQuery(QString("syscalls[@version='1.0']/linux[@arch='" + arch + "']/syscall[index=%1]").arg(regAX));
		if (query.isValid()) {
			query.evaluateTo(&syscall_entry);
		}
		file.close();
	}

	if (!syscall_entry.isEmpty()) {
		syscall_xml.setContent("" + syscall_entry + "");
		QDomElement root = syscall_xml.documentElement();

		QStringList arguments;

		for (QDomElement argument = root.firstChildElement("argument"); !argument.isNull(); argument = argument.nextSiblingElement("argument")) {
			const QString argument_type     = argument.attribute("type");
			const QString argument_register = argument.attribute("register");
			if (argument_register == "ebp" && inst.operation() == X86_INS_SYSENTER) {
				if (IProcess *process = edb::v1::debugger_core->process()) {
					char buf[4];
					if (process->readBytes(state.stackPointer(), buf, sizeof(buf)) != sizeof(buf)) {
						arguments << QObject::tr("(failed to read [esp])");
						continue;
					}
					std::uint32_t value;
					std::memcpy(&value, buf, sizeof(value));
					arguments << format_argument(argument_type, make_Register<32>("[esp]", value, Register::TYPE_GPR));
					continue;
				}
			}
			const auto reg = state[argument_register];
			if (reg) {
				arguments << format_argument(argument_type, reg);
				continue;
			} else {
				// If we failed, this may be a pair of reg32a:reg32b
				const auto regs = argument_register.split(':');
				if (regs.size() != 2 || regs[0].isEmpty() || regs[0][0] != 'e' || regs[1].isEmpty() || regs[1][0] != 'e') {
					arguments << QObject::tr("(failed to obtain %1)").arg(argument_register);
					continue;
				}
				const auto regHi = state[regs[0]], regLo = state[regs[1]];
				if (!regHi || !regLo || regHi.bitSize() != 32 || regLo.bitSize() != 32) {
					arguments << QObject::tr("(failed to obtain %1)").arg(argument_register);
					continue;
				}
				const auto value = regHi.valueAsInteger() << 32 | regLo.valueAsInteger();
				arguments << format_argument(argument_type, make_Register<64>(argument_register, value, Register::TYPE_GPR));
			}
		}

		ret << ArchProcessor::tr("SYSCALL: %1%2(%3)").arg(isX32 ? "x32:" : "", root.attribute("name"), arguments.join(","));
	}
#else
	Q_UNUSED(regAX)
#endif
}

QString gprComment(const Register &reg) {
	QString regString;
	int stringLength;
	QString comment;
	if (edb::v1::get_ascii_string_at_address(reg.valueAsAddress(), regString, edb::v1::config().min_string_length, 256, stringLength))
		comment = QString("ASCII \"%1\"").arg(regString);
	else if (edb::v1::get_utf16_string_at_address(reg.valueAsAddress(), regString, edb::v1::config().min_string_length, 256, stringLength))
		comment = QString("UTF16 \"%1\"").arg(regString);
	return comment;
}

RegisterViewModel &getModel() {
	return static_cast<RegisterViewModel &>(edb::v1::arch_processor().registerViewModel());
}

void updateGPRs(RegisterViewModel &model, const State &state, bool is64Bit) {
	if (is64Bit) {
		for (std::size_t i = 0; i < GPR64_COUNT; ++i) {
			const auto reg = state.gpRegister(i);
			if (!reg) {
				continue;
			}

			Q_ASSERT(reg.bitSize() == 64);
			QString comment;
			if (i == 0) {
				const auto origAX = state["orig_rax"].valueAsSignedInteger();
				if (origAX != -1) {
					comment            = "orig: " + edb::value64(origAX).toHexString();
					const auto errName = syscallErrName(reg.value<edb::value64>());
					if (!errName.isEmpty())
						comment = "-" + errName + "; " + comment;
				}
			}
			if (comment.isEmpty())
				comment = gprComment(reg);
			model.updateGPR(i, reg.value<edb::value64>(), comment);
		}
	} else {
		for (std::size_t i = 0; i < GPR32_COUNT; ++i) {
			const auto reg = state.gpRegister(i);
			if (!reg) {
				continue;
			}
			Q_ASSERT(reg.bitSize() == 32);
			QString comment;
			if (i == 0) {
				const auto origAX = state["orig_eax"].valueAsSignedInteger();
				if (origAX != -1) {
					comment            = "orig: " + edb::value32(origAX).toHexString();
					const auto errName = syscallErrName(reg.value<edb::value32>());
					if (!errName.isEmpty())
						comment = "-" + errName + "; " + comment;
				}
			}
			if (comment.isEmpty())
				comment = gprComment(reg);
			model.updateGPR(i, reg.value<edb::value32>(), comment);
		}
	}
}

QString rIPcomment(edb::address_t rIP, const QString &default_region_name) {
	const auto symname = edb::v1::find_function_symbol(rIP, default_region_name);
	return symname.isEmpty() ? symname : '<' + symname + '>';
}

QString eflagsComment(edb::reg_t flags) {
	QString comment = "(";
	for (int cond = 0; cond < 0x10; ++cond)
		if (is_jcc_taken(flags, static_cast<edb::Instruction::ConditionCode>(cond)))
			comment += jumpConditionMnemonics[cond] + ',';
	comment[comment.size() - 1] = ')';
	return comment;
}

void updateGeneralStatusRegs(RegisterViewModel &model, const State &state, bool is64Bit, const QString &default_region_name) {
	const auto ip    = state.instructionPointerRegister();
	const auto flags = state.flagsRegister();
	Q_ASSERT(!!ip);
	Q_ASSERT(!!flags);
	const auto ipComment    = rIPcomment(ip.valueAsAddress(), default_region_name);
	const auto flagsComment = eflagsComment(flags.valueAsInteger());
	if (is64Bit) {
		model.updateIP(ip.value<edb::value64>(), ipComment);
		model.updateFlags(flags.value<edb::value64>(), flagsComment);
	} else {
		model.updateIP(ip.value<edb::value32>(), ipComment);
		model.updateFlags(flags.value<edb::value32>(), flagsComment);
	}
}

QString FPUStackFaultDetail(uint16_t statusWord) {
	const bool invalidOperationException = statusWord & 0x01;
	const bool C1                        = statusWord & (1 << 9);
	const bool stackFault                = statusWord & 0x40;
	if (invalidOperationException && stackFault)
		return C1 ? QObject::tr("Stack overflow") : QObject::tr("Stack underflow");
	return "";
}

QString FPUComparExplain(uint16_t statusWord) {
	const bool C0 = statusWord & (1 << 8);
	const bool C2 = statusWord & (1 << 10);
	const bool C3 = statusWord & (1 << 14);
	if (C3 == 0 && C2 == 0 && C0 == 0) return "GT";
	if (C3 == 0 && C2 == 0 && C0 == 1) return "LT";
	if (C3 == 1 && C2 == 0 && C0 == 0) return "EQ";
	if (C3 == 1 && C2 == 1 && C0 == 1) return QObject::tr("Unordered", "result of FPU comparison instruction");
	return "";
}

QString FPUExplainPE(uint16_t statusWord) {
	if (statusWord & (1 << 5)) {
		const bool C1 = statusWord & (1 << 9);
		return C1 ? QObject::tr("Rounded UP") : QObject::tr("Rounded DOWN");
	}
	return "";
}

QString FSRComment(uint16_t statusWord) {

	const auto stackFaultDetail = FPUStackFaultDetail(statusWord);
	const auto comparisonResult = FPUComparExplain(statusWord);
	const auto comparComment    = comparisonResult.isEmpty() ? "" : '(' + comparisonResult + ')';
	const auto peExplanation    = FPUExplainPE(statusWord);

	auto comment = comparComment;
	if (comment.length() && stackFaultDetail.length()) comment += ", ";
	comment += stackFaultDetail;
	if (comment.length() && peExplanation.length()) comment += ", ";
	comment += peExplanation;
	return comment.trimmed();
}

void updateSegRegs(RegisterViewModel &model, const State &state) {
	static const QString sregs[] = {"es", "cs", "ss", "ds", "fs", "gs"};
	for (std::size_t i = 0; i < sizeof(sregs) / sizeof(sregs[0]); ++i) {
		QString sreg(sregs[i]);
		const auto sregValue = state[sreg].value<edb::seg_reg_t>();
		const Register base  = state[sregs[i] + "_base"];
		QString comment;
		if (edb::v1::debuggeeIs32Bit() || i >= FS) {
			if (base)
				comment = QString("(%1)").arg(base.valueAsAddress().toHexString());
			else if (edb::v1::debuggeeIs32Bit() && sregValue == 0)
				comment = "NULL";
			else
				comment = "(?)";
		}
		model.updateSegReg(i, sregValue, comment);
	}
}

void updateFPURegs(RegisterViewModel &model, const State &state) {
	for (std::size_t i = 0; i < MAX_FPU_REGS_COUNT; ++i) {
		const auto reg     = state.fpuRegister(i);
		const auto comment = float_type(reg) == FloatValueClass::PseudoDenormal ? QObject::tr("pseudo-denormal") : "";
		model.updateFPUReg(i, reg, comment);
	}
	model.updateFCR(state.fpuControlWord());
	const auto fsr = state.fpuStatusWord();
	model.updateFSR(fsr, FSRComment(fsr));
	model.updateFTR(state.fpuTagWord());
	{
		const Register FIS = state["FIS"];
		if (FIS)
			model.updateFIS(FIS.value<edb::value16>());
		else
			model.invalidateFIS();
	}
	{
		const Register FDS = state["FDS"];
		if (FDS)
			model.updateFDS(FDS.value<edb::value16>());
		else
			model.invalidateFDS();
	}
	{
		const Register FIP = state["FIP"];
		if (FIP.bitSize() == 64)
			model.updateFIP(FIP.value<edb::value64>());
		else if (FIP.bitSize() == 32)
			model.updateFIP(FIP.value<edb::value32>());
		else
			model.invalidateFIP();
	}
	{
		const Register FDP = state["FDP"];
		if (FDP.bitSize() == 64)
			model.updateFDP(FDP.value<edb::value64>());
		else if (FDP.bitSize() == 32)
			model.updateFDP(FDP.value<edb::value32>());
		else
			model.invalidateFDP();
	}
	{
		const Register FOP = state["fopcode"];
		if (FOP) {
			const auto value = FOP.value<edb::value16>();
			// Yes, FOP is a big-endian view of the instruction
			const auto comment = value > 0x7ff ? QString("?!!") : QObject::tr("Insn: %1 %2").arg((edb::value8(value >> 8) + 0xd8).toHexString(), edb::value8(value).toHexString());
			model.updateFOP(value, comment);
		} else
			model.invalidateFOP();
	}
}

void updateDebugRegs(RegisterViewModel &model, const State &state) {
	for (std::size_t i = 0; i < MAX_DEBUG_REGS_COUNT; ++i) {
		const edb::reg_t reg = state.debugRegister(i);
		if (edb::v1::debuggeeIs32Bit())
			model.updateDR(i, edb::value32(reg));
		else
			model.updateDR(i, reg);
	}
}

void updateMMXRegs(RegisterViewModel &model, const State &state) {
	for (std::size_t i = 0; i < MAX_MMX_REGS_COUNT; ++i) {
		const auto reg = state.archRegister(edb::string_hash("mmx"), i);

		if (!!reg) {
			model.updateMMXReg(i, reg.value<MMWord>());
		} else {
			model.invalidateMMXReg(i);
		}
	}
}

void updateSSEAVXRegs(RegisterViewModel &model, const State &state, bool hasSSE, bool hasAVX) {

	if (!hasSSE) {
		return;
	}

	const std::size_t max = edb::v1::debuggeeIs32Bit() ? AVX32_COUNT : AVX64_COUNT;

	for (std::size_t i = 0; i < max; ++i) {
		if (hasAVX) {
			const auto reg = state.archRegister(edb::string_hash("ymm"), i);
			if (!reg) {
				model.invalidateAVXReg(i);
			} else {
				model.updateAVXReg(i, reg.value<YMMWord>());
			}
		} else if (hasSSE) {
			const auto reg = state.archRegister(edb::string_hash("xmm"), i);
			if (!reg) {
				model.invalidateSSEReg(i);
			} else {
				model.updateSSEReg(i, reg.value<XMMWord>());
			}
		}
	}

	const auto mxcsr = state["mxcsr"];
	if (!mxcsr) {
		model.invalidateMXCSR();
	} else {
		model.updateMXCSR(mxcsr.value<edb::value32>());
	}
}

bool falseSyscallReturn(const State &state, std::int64_t origAX) {
	// Prevent reporting of returns from execve() when the process has just launched
	if (EDB_IS_32_BIT && origAX == 11) {
		return state.gpRegister(rAX).valueAsInteger() == 0 &&
			   state.gpRegister(rCX).valueAsInteger() == 0 &&
			   state.gpRegister(rDX).valueAsInteger() == 0 &&
			   state.gpRegister(rBX).valueAsInteger() == 0 &&
			   state.gpRegister(rBP).valueAsInteger() == 0 &&
			   state.gpRegister(rSI).valueAsInteger() == 0 &&
			   state.gpRegister(rDI).valueAsInteger() == 0;
	} else if (EDB_IS_64_BIT && origAX == 59) {
		return state.gpRegister(rAX).valueAsInteger() == 0 &&
			   state.gpRegister(rCX).valueAsInteger() == 0 &&
			   state.gpRegister(rDX).valueAsInteger() == 0 &&
			   state.gpRegister(rBX).valueAsInteger() == 0 &&
			   state.gpRegister(rBP).valueAsInteger() == 0 &&
			   state.gpRegister(rSI).valueAsInteger() == 0 &&
			   state.gpRegister(rDI).valueAsInteger() == 0 &&
			   state.gpRegister(R8).valueAsInteger() == 0 &&
			   state.gpRegister(R9).valueAsInteger() == 0 &&
			   state.gpRegister(R10).valueAsInteger() == 0 &&
			   state.gpRegister(R11).valueAsInteger() == 0 &&
			   state.gpRegister(R12).valueAsInteger() == 0 &&
			   state.gpRegister(R13).valueAsInteger() == 0 &&
			   state.gpRegister(R14).valueAsInteger() == 0 &&
			   state.gpRegister(R15).valueAsInteger() == 0;
	}
	return false;
}

}

//------------------------------------------------------------------------------
// Name: getEffectiveAddress
// Desc:
//------------------------------------------------------------------------------
Result<edb::address_t, QString> ArchProcessor::getEffectiveAddress(const edb::Instruction &inst, const edb::Operand &op, const State &state) const {

	edb::address_t ret = 0;
	// TODO: get registers by index, not string! too slow

	if (op) {
		if (is_register(op)) {
			ret = state[QString::fromStdString(edb::v1::formatter().toString(op))].valueAsAddress();
		} else if (is_expression(op)) {
			const Register baseR  = state[QString::fromStdString(register_name(op->mem.base))];
			const Register indexR = state[QString::fromStdString(register_name(op->mem.index))];
			edb::address_t base   = 0;
			edb::address_t index  = 0;

			if (!baseR) {
				if (op->mem.base != X86_REG_INVALID)
					return make_unexpected(tr("failed to acquire base register from state"));
			} else {
				base = baseR.valueAsAddress();
			}

			if (!indexR) {
				if (op->mem.index != X86_REG_INVALID)
					return make_unexpected(tr("failed to acquire index register from state"));
			} else {
				if (indexR.type() != Register::TYPE_GPR)
					return make_unexpected(tr("only general-purpose register is supported as index register"));
				index = indexR.valueAsAddress();
			}

			// This only makes sense for x86_64, but doesn't hurt on x86
			if (op->mem.base == X86_REG_RIP) {
				base += inst.byteSize();
			}

			ret = base + index * op->mem.scale + op->mem.disp;

			std::size_t segRegIndex = op->mem.segment;

			// handle implicit segments on 32-bit (capstone doesn't call them out explicitly)
			if (segRegIndex == X86_REG_INVALID && !debuggeeIs64Bit()) {
				switch (op->mem.base) {
				case X86_REG_BP:
				case X86_REG_SP:
				case X86_REG_EBP:
				case X86_REG_ESP:
					segRegIndex = X86_REG_SS;
					break;
				default:
					segRegIndex = X86_REG_DS;
					break;
				}
			}

			if (segRegIndex != X86_REG_INVALID) {

				const Register segBase = [&segRegIndex, &state]() {
					switch (segRegIndex) {
					case X86_REG_ES:
						return state[QLatin1String("es_base")];
					case X86_REG_CS:
						return state[QLatin1String("cs_base")];
					case X86_REG_SS:
						return state[QLatin1String("ss_base")];
					case X86_REG_DS:
						return state[QLatin1String("ds_base")];
					case X86_REG_FS:
						return state[QLatin1String("fs_base")];
					case X86_REG_GS:
						return state[QLatin1String("gs_base")];
					default:
						return Register();
					}
				}();

				if (!segBase) return make_unexpected(QObject::tr("failed to obtain segment base")); // no way to reliably compute address
				ret += segBase.valueAsAddress();
			}
		} else if (is_immediate(op)) {
			const Register csBase = state["cs_base"];
			if (!csBase) return make_unexpected(QObject::tr("failed to obtain CS segment base")); // no way to reliably compute address
			ret = op->imm + csBase.valueAsAddress();
		}
	}

	ret.normalize();
	return ret;
}

edb::address_t ArchProcessor::getEffectiveAddress(const edb::Instruction &inst, const edb::Operand &op, const State &state, bool &ok) const {

	ok                = false;
	const auto result = getEffectiveAddress(inst, op, state);
	if (!result) return 0;
	ok = true;
	return result.value();
}

//------------------------------------------------------------------------------
// Name: ArchProcessor
// Desc:
//------------------------------------------------------------------------------
ArchProcessor::ArchProcessor() {
	if (edb::v1::debugger_core) {
		hasMmx_ = edb::v1::debugger_core->hasExtension(edb::string_hash("MMX"));
		hasXmm_ = edb::v1::debugger_core->hasExtension(edb::string_hash("XMM"));
		hasYmm_ = edb::v1::debugger_core->hasExtension(edb::string_hash("YMM"));
		connect(edb::v1::debugger_ui, SIGNAL(attachEvent()), this, SLOT(justAttached()));
	} else {
		hasMmx_ = false;
		hasXmm_ = false;
		hasYmm_ = false;
	}
}

//------------------------------------------------------------------------------
// Name: setup_register_view
// Desc:
//------------------------------------------------------------------------------
void ArchProcessor::setupRegisterView() {

	if (edb::v1::debugger_core) {

		updateRegisterView(QString(), State());
	}
}

//------------------------------------------------------------------------------
// Name: reset
// Desc:
//------------------------------------------------------------------------------
void ArchProcessor::reset() {

	if (edb::v1::debugger_core) {
		updateRegisterView(QString(), State());
	}
}

//------------------------------------------------------------------------------
// Name: update_register_view
// Desc:
//------------------------------------------------------------------------------
void ArchProcessor::updateRegisterView(const QString &default_region_name, const State &state) {

	auto &model = getModel();

	const auto ip = state.instructionPointerRegister();

	if (!ip) {
		model.setCpuMode(RegisterViewModel::CpuMode::UNKNOWN);
		return;
	}
	const bool is64Bit = (ip.bitSize() == 64);
	Q_ASSERT(is64Bit || ip.bitSize() == 32);

	model.setCpuMode(is64Bit ? RegisterViewModel::CpuMode::AMD64 : RegisterViewModel::CpuMode::IA32);
	updateGPRs(model, state, is64Bit);
	updateGeneralStatusRegs(model, state, is64Bit, default_region_name);
	updateSegRegs(model, state);
	updateFPURegs(model, state);
	updateDebugRegs(model, state);
	updateMMXRegs(model, state);
	updateSSEAVXRegs(model, state, hasXmm_, hasYmm_);

	if (justAttached_) {
		model.saveValues();
		justAttached_ = false;
	}
	model.dataUpdateFinished();
}

void ArchProcessor::aboutToResume() {
	getModel().saveValues();
}

void ArchProcessor::justAttached() {
	justAttached_ = true;
}

//------------------------------------------------------------------------------
// Name: update_instruction_info
// Desc:
//------------------------------------------------------------------------------
QStringList ArchProcessor::updateInstructionInfo(edb::address_t address) {

	QStringList ret;

	Q_ASSERT(edb::v1::debugger_core);

	if (IProcess *process = edb::v1::debugger_core->process()) {
		uint8_t buffer[edb::Instruction::MaxSize];

		if (process->readBytes(address, buffer, sizeof(buffer))) {
			edb::Instruction inst(buffer, buffer + sizeof(buffer), address);
			if (inst) {

				State state;
				process->currentThread()->getState(&state);

				std::int64_t origAX;
				if (debuggeeIs64Bit()) {
					origAX = state["orig_rax"].valueAsSignedInteger();
				} else {
					origAX = state["orig_eax"].valueAsSignedInteger();
				}

				const std::uint64_t rax = state.gpRegister(rAX).valueAsSignedInteger();

				if (origAX != -1 && !falseSyscallReturn(state, origAX)) {

					// FIXME: this all doesn't work correctly when we're on the first instruction of a signal handler
					// The registers there don't correspond to arguments of the syscall, and it's not correct to say the
					// debuggee _returned_ from the syscall, since it's just interrupted the syscall to handle the signal
					analyze_syscall(state, inst, ret, origAX);
#ifdef Q_OS_LINUX
					enum {
						// restart if no handler or if SA_RESTART is set, can be seen when interrupting e.g. waitpid
						ERESTARTSYS = 512,
						// restart unconditionally
						ERESTARTNOINTR = 513,
						// restart if no handler
						ERESTARTNOHAND = 514,
						// restart by sys_restart_syscall, can be seen when interrupting e.g. nanosleep
						ERESTART_RESTARTBLOCK = 516,
					};
					const auto err         = rax >= -4095UL ? -rax : 0;
					const bool interrupted = err == EINTR ||
											 err == ERESTARTSYS ||
											 err == ERESTARTNOINTR ||
											 err == ERESTARTNOHAND ||
											 err == ERESTART_RESTARTBLOCK;

					if (ret.size() && ret.back().startsWith("SYSCALL")) {
						if (interrupted)
							ret.back() = "Interrupted " + ret.back();
						else
							ret.back() = "Returned from " + ret.back();
					}
					// FIXME: actually only ERESTARTNOINTR guarantees reexecution. But it seems the other ERESTART* signals
					// won't go into user space, so whatever the state of signal handlers, the tracee should never appear
					// to see these signals. So I guess it's OK to assume that tha syscall _will_ be restarted by the kernel.
					if (interrupted && err != EINTR)
						ret << QString("Syscall will be restarted on next step/run");
#else
					Q_UNUSED(rax)
#endif
				}

				// figure out the instruction type and display some information about it
				// TODO: handle SETcc, LOOPcc, REPcc OP
				if (is_conditional_move(inst)) {

					analyze_cmov(state, inst, ret);

				} else if (is_ret(inst)) {

					analyze_return(state, inst, ret);

				} else if (is_jump(inst) || is_call(inst)) {

					if (is_conditional_jump(inst))
						analyze_jump(state, inst, ret);
					analyze_call(state, inst, ret);
				} else if (is_int(inst)) {
#ifdef Q_OS_LINUX
					if ((inst[0]->imm & 0xff) == 0x80) {

						analyze_syscall(state, inst, ret, state.gpRegister(rAX).valueAsInteger());
					} else {

						analyze_operands(state, inst, ret);
					}
#endif
				} else if (is_syscall(inst) || is_sysenter(inst)) {

					analyze_syscall(state, inst, ret, state.gpRegister(rAX).valueAsInteger());

				} else {

					analyze_operands(state, inst, ret);
				}

				analyze_jump_targets(inst, ret);
			}
		}

		// eliminate duplicates
		ret.sort();
		ret.erase(std::unique(ret.begin(), ret.end()), ret.end());
	}
	return ret;
}

//------------------------------------------------------------------------------
// Name: can_step_over
// Desc:
//------------------------------------------------------------------------------
bool ArchProcessor::canStepOver(const edb::Instruction &inst) const {
	return inst && (is_call(inst) || is_repeat(inst));
}

//------------------------------------------------------------------------------
// Name: is_filling
// Desc:
//------------------------------------------------------------------------------
bool ArchProcessor::isFilling(const edb::Instruction &inst) const {
	bool ret = false;

	// fetch the operands
	if (inst) {

		switch (inst.operation()) {
		case X86_INS_NOP:
		case X86_INS_INT3:
			ret = true;
			break;

		case X86_INS_LEA:

			Q_ASSERT(inst.operandCount() >= 2);

			if (is_register(inst[0]) && is_expression(inst[1])) {
				int reg1 = inst[0]->reg;

				if (inst[1]->mem.scale == 1) {
					if (inst[1]->mem.disp == 0) {
						int reg2;
						if (inst[1]->mem.base == X86_REG_INVALID) {
							reg2 = inst[1]->mem.index;
							ret  = (reg1 == reg2);
						} else if (inst[1]->mem.index == X86_REG_INVALID) {
							reg2 = inst[1]->mem.base;
							ret  = (reg1 == reg2);
						}
					}
				}
			}
			break;

		case X86_INS_MOV:

			Q_ASSERT(inst.operandCount() >= 2);

			if (is_register(inst[0]) && is_register(inst[1])) {
				ret = (inst[0]->reg == inst[1]->reg);
			}
			break;

		default:
			break;
		}

		if (!ret) {
			if (edb::v1::config().zeros_are_filling) {
				ret = (QByteArray::fromRawData(reinterpret_cast<const char *>(inst.bytes()), inst.byteSize()) == QByteArray::fromRawData("\x00\x00", 2));
			}
		}
	} else {
		ret = (inst.byteSize() == 1 && inst.bytes()[0] == 0x00);
	}

	return ret;
}

//------------------------------------------------------------------------------
// Name: register_view_model
// Desc:
//------------------------------------------------------------------------------
RegisterViewModelBase::Model &ArchProcessor::registerViewModel() const {
	static RegisterViewModel model(hasMmx_ * RegisterViewModel::CPUFeatureBits::MMX |
								   hasXmm_ * RegisterViewModel::CPUFeatureBits::SSE |
								   hasYmm_ * RegisterViewModel::CPUFeatureBits::AVX);
	return model;
}

bool ArchProcessor::isExecuted(const edb::Instruction &inst, const State &state) const {
	return is_jcc_taken(state, inst.conditionCode());
}
