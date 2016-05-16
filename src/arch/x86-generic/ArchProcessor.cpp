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
#include "IDebugger.h"
#include "Instruction.h"
#include "Prototype.h"
#include "RegisterListWidget.h"
#include "State.h"
#include "Util.h"
#include "edb.h"
#include "string_hash.h"

#include <QApplication>
#include <QDebug>
#include <QDomDocument>
#include <QFile>
#include <QVector>
#include <QXmlQuery>
#include <QMenu>
#include <QSignalMapper>

#include <climits>
#include <cmath>
#include <memory>
#include <cstring>

#include "FloatX.h"
#include "RegisterViewModel.h"

#ifdef Q_OS_LINUX
#include <asm/unistd.h>
#include "errno-names-linux.h"
#endif

namespace {

using std::size_t;

enum RegisterIndex {
	rAX  = 0,
	rCX  = 1,
	rDX  = 2,
	rBX  = 3,
	rSP  = 4,
	rBP  = 5,
	rSI  = 6,
	rDI  = 7,
	R8   = 8,
	R9   = 9,
	R10  = 10,
	R11  = 11,
	R12  = 12,
	R13  = 13,
	R14  = 14,
	R15  = 15
};

enum SegmentRegisterIndex {
	ES,
	CS,
	SS,
	DS,
	FS,
	GS
};

static constexpr size_t MAX_DEBUG_REGS_COUNT=8;
static constexpr size_t MAX_SEGMENT_REGS_COUNT=6;
static constexpr size_t GPR32_COUNT=8;
static constexpr size_t GPR64_COUNT=16;
static constexpr size_t SSE32_COUNT=GPR32_COUNT;
static constexpr size_t SSE64_COUNT=GPR64_COUNT;
static constexpr size_t AVX32_COUNT=SSE32_COUNT;
static constexpr size_t AVX64_COUNT=SSE64_COUNT;
static constexpr size_t MAX_GPR_COUNT=GPR64_COUNT;
static constexpr size_t MAX_FPU_REGS_COUNT=8;
static constexpr size_t MAX_MMX_REGS_COUNT=MAX_FPU_REGS_COUNT;
static constexpr size_t XMM_REGS_COUNT=MAX_GPR_COUNT;
static constexpr size_t YMM_REGS_COUNT=MAX_GPR_COUNT;
using edb::v1::debuggeeIs32Bit;
using edb::v1::debuggeeIs64Bit;
int func_param_regs_count() { return debuggeeIs32Bit() ? 0 : 6; }

typedef edb::value64 MMWord;
typedef edb::value128 XMMWord;
typedef edb::value256 YMMWord;
typedef edb::value512 ZMMWord;

template<typename T>
std::string register_name(const T& val) {
	return edb::v1::formatter().register_name(val);
}

template<typename T>
QString syscallErrName(T err) {
#ifdef Q_OS_LINUX
	std::size_t index=-err;
	if(index>=sizeof errnoNames/sizeof*errnoNames) return "";
	if(errnoNames[index]) return errnoNames[index];
    return "";
#else
	return "";
#endif
}

//------------------------------------------------------------------------------
// Name: get_effective_address
// Desc:
//------------------------------------------------------------------------------
edb::address_t get_effective_address(const edb::Operand &op, const State &state, bool& ok) {
	edb::address_t ret = 0;
	ok=false;

	// TODO: get registers by index, not string! too slow
	
	if(op.valid()) {
		switch(op.general_type()) {
		case edb::Operand::TYPE_REGISTER:
			ret = state[QString::fromStdString(edb::v1::formatter().to_string(op))].valueAsAddress();
			break;
		case edb::Operand::TYPE_EXPRESSION:
			do {
				const Register baseR  = state[QString::fromStdString(register_name(op.expression().base))];
				const Register indexR = state[QString::fromStdString(register_name(op.expression().index))];
				edb::address_t base  = 0;
				edb::address_t index = 0;

				if(!baseR)
				{
					if(op.expression().base!=edb::Operand::Register::X86_REG_INVALID)
						return ret; // register is valid, but failed to be acquired from state
				}
				else
				{
					base=baseR.valueAsAddress();
				}

				if(!indexR)
				{
					if(op.expression().index!=edb::Operand::Register::X86_REG_INVALID)
						return ret; // register is valid, but failed to be acquired from state
				}
				else
				{
					if(indexR.type()!=Register::TYPE_GPR)
						return ret; // TODO: add support for VSIB addressing
					index=indexR.valueAsAddress();
				}

				// This only makes sense for x86_64, but doesn't hurt on x86
				if(op.expression().base == edb::Operand::Register::X86_REG_RIP) {
					base += op.owner()->size();
				}

				ret = base + index * op.expression().scale + op.displacement();

				std::size_t segRegIndex=op.expression().segment;
				if(segRegIndex!=edb::Operand::Segment::REG_INVALID) {
					static constexpr const char segInitials[]="ecsdfg";
					Q_ASSERT(segRegIndex<sizeof(segInitials));
					const Register segBase=state[segInitials[segRegIndex]+QString("s_base")];
					if(!segBase) return 0; // no way to reliably compute address
					ret += segBase.valueAsAddress();
				}
			} while(0);
			break;
		case edb::Operand::TYPE_ABSOLUTE:
			// TODO: find out segment base for op.absolute().seg, otherwise this isn't going to be useful
			ret = op.absolute().offset;
			break;
		case edb::Operand::TYPE_IMMEDIATE:
			break;
		case edb::Operand::TYPE_REL:
			{
				const Register csBase = state["cs_base"];
				if(!csBase) return 0; // no way to reliably compute address
				ret = op.relative_target() + csBase.valueAsAddress();
				break;
			}
		default:
			break;
		}
	}
	ok=true;
	ret.normalize();
	return ret;
}

//------------------------------------------------------------------------------
// Name: format_pointer
// Desc:
//------------------------------------------------------------------------------
QString format_pointer(int pointer_level, edb::reg_t arg, QChar type) {

	Q_UNUSED(type);
	Q_UNUSED(pointer_level);

	if(arg == 0) {
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
	if(pointer_level > 0) {
		return format_pointer(pointer_level, arg, type);
	}

	QString s;

	switch(type.toLatin1()) {
	case 'w': return s.sprintf("%u", static_cast<wchar_t>(arg));
	case 'b': return s.sprintf("%d", static_cast<bool>(arg));
	case 'c':
		if(arg < 0x80u && (std::isprint(arg) || std::isspace(arg))) {
			return s.sprintf("'%c'", static_cast<char>(arg));
		} else {
			return s.sprintf("'\\x%02x'", static_cast<quint16>(arg));
		}


	case 'a': return s.sprintf("%d", static_cast<signed char>(arg));
	case 'h': return s.sprintf("%u", static_cast<unsigned char>(arg));
	case 's': return s.sprintf("%d", static_cast<short>(arg));
	case 't': return s.sprintf("%u", static_cast<unsigned short>(arg));
	case 'i': return s.sprintf("%d", static_cast<int>(arg));
	case 'j': return s.sprintf("%u", static_cast<unsigned int>(arg));
	case 'l': return s.sprintf("%ld", static_cast<long>(arg));
	case 'm': return s.sprintf("%lu", static_cast<unsigned long>(arg));
	case 'x': return s.sprintf("%lld", static_cast<long long>(arg));
	case 'y': return s.sprintf("%llu", static_cast<long unsigned long>(arg));
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

	if(IProcess *process = edb::v1::debugger_core->process()) {
		if(pointer_level == 1) {
			if(arg == 0) {
				return "NULL";
			} else {
				QString string_param;
				int string_length;

				if(edb::v1::get_ascii_string_at_address(arg, string_param, edb::v1::config().min_string_length, 256, string_length)) {
					return QString("<%1> \"%2\"").arg(edb::v1::format_pointer(arg)).arg(string_param);
				} else {
					char character;
					process->read_bytes(arg, &character, sizeof(character));
					if(character == '\0') {
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
QString format_argument(const QString &type, const Register& arg) {
	QString param_text;

	int pointer_level = 0;
	for(QChar ch: type) {

		if(ch == 'P') {
			++pointer_level;
		} else if(ch == 'r' || ch == 'V' || ch == 'K') {
			// skip things like const, volatile, restrict, they don't effect
			// display for us
			continue;
		} else {
			switch(ch.toLatin1()) {
			case 'v': return format_pointer(pointer_level, arg.valueAsAddress(), ch);
			case 'w': return format_integer(pointer_level, arg.valueAsInteger(), ch);
			case 'b': return format_integer(pointer_level, arg.valueAsSignedInteger(), ch);
			case 'c': return format_char(pointer_level, arg.valueAsAddress(), ch);
			case 'a': return format_integer(pointer_level, arg.valueAsSignedInteger(), ch);
			case 'h': return format_integer(pointer_level, arg.valueAsInteger(), ch);
			case 's': return format_integer(pointer_level, arg.valueAsSignedInteger(), ch);
			case 't': return format_integer(pointer_level, arg.valueAsInteger(), ch);
			case 'i': return format_integer(pointer_level, arg.valueAsSignedInteger(), ch);
			case 'j': return format_integer(pointer_level, arg.valueAsInteger(), ch);
			case 'l': return format_integer(pointer_level, arg.valueAsSignedInteger(), ch);
			case 'm': return format_integer(pointer_level, arg.valueAsInteger(), ch);
			case 'x': return format_integer(pointer_level, arg.valueAsSignedInteger(), ch);
			case 'y': return format_integer(pointer_level, arg.valueAsInteger(), ch);
			case 'n': return format_integer(pointer_level, arg.valueAsSignedInteger(), ch);
			case 'o': return format_integer(pointer_level, arg.valueAsSignedInteger(), ch);
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
	const std::vector<const char *> parameter_registers = ( debuggeeIs64Bit() ? std::vector<const char*>{
		"rdi",
		"rsi",
		"rdx",
		"rcx",
		"r8",
		"r9" } :
	std::vector<const char*>{} );

	if(IProcess *process = edb::v1::debugger_core->process()) {
		// we will always be removing the last 2 chars '+0' from the string as well
		// as chopping the region prefix we like to prepend to symbols
		QString func_name;
		const int colon_index = symname.indexOf("::");

		if(colon_index != -1) {
			func_name = symname.left(symname.length() - 2).mid(colon_index + 2);
		}

		// safe not to check for -1, it means 'rest of string' for the mid function
		func_name = func_name.mid(0, func_name.indexOf("@"));

		if(const edb::Prototype *const info = edb::v1::get_function_info(func_name)) {

			QStringList arguments;
			int i = 0;
			for(edb::Argument argument: info->arguments) {

				Register arg;
				if(i+1 > func_param_regs_count()) {
					size_t arg_i_position=(i - func_param_regs_count()) * edb::v1::pointer_size();
					edb::reg_t value(0);
					process->read_bytes(state.stack_pointer() + offset + arg_i_position, &value, edb::v1::pointer_size());
					arg=edb::v1::debuggeeIs64Bit()?
						make_Register<64>("",value,Register::TYPE_GPR) :
						make_Register<32>("",value,Register::TYPE_GPR);
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

	switch(cond & 0x0e) {
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

	if(cond & 0x01) {
		taken = !taken;
	}

	return taken;
}

//------------------------------------------------------------------------------
// Name: is_jcc_taken
// Desc:
//------------------------------------------------------------------------------
bool is_jcc_taken(const State &state, edb::Instruction::ConditionCode cond) {

	if(cond==edb::Instruction::CC_UNCONDITIONAL) return true;
	if(cond==edb::Instruction::CC_RCXZ) return state.gp_register(rCX).value<edb::value64>() == 0;
	if(cond==edb::Instruction::CC_ECXZ) return state.gp_register(rCX).value<edb::value32>() == 0;
	if(cond==edb::Instruction::CC_CXZ)  return state.gp_register(rCX).value<edb::value16>() == 0;

	return is_jcc_taken(state.flags(),cond);
}

static QString jumpConditionMnemonics[]={
										 "O", "NO",
										 "B", "AE",
										 "E", "NE",
										 "BE", "A",
										 "S", "NS",
										 "P", "NP",
										 "L", "GE",
										 "LE", "G"
										 };

//------------------------------------------------------------------------------
// Name: analyze_cmov
// Desc:
//------------------------------------------------------------------------------
void analyze_cmov(const State &state, const edb::Instruction &inst, QStringList &ret) {

	const bool taken = is_jcc_taken(state, inst.condition_code());

	if(taken) {
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

	if(is_conditional_jump(inst)) {
		taken = is_jcc_taken(state, inst.condition_code());
	}

	if(taken) {
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

	Q_UNUSED(inst);

	if(IProcess *process = edb::v1::debugger_core->process()) {
		edb::address_t return_address(0);
		process->read_bytes(state.stack_pointer(), &return_address, edb::v1::pointer_size());
	
		const QString symname = edb::v1::find_function_symbol(return_address);
		if(!symname.isEmpty()) {
			ret << ArchProcessor::tr("return to %1 <%2>").arg(edb::v1::format_pointer(return_address)).arg(symname);
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

	if(IProcess *process = edb::v1::debugger_core->process()) {
		const edb::Operand &operand = inst.operands()[0];

		if(operand.valid()) {
		
			bool ok;
			const edb::address_t effective_address = get_effective_address(operand, state,ok);
			if(!ok) return;
			const QString temp_operand             = QString::fromStdString(edb::v1::formatter().to_string(operand));
			QString temp;

			switch(operand.general_type()) {
			case edb::Operand::TYPE_REL:
				do {
					int offset;
					const QString symname = edb::v1::find_function_symbol(effective_address, QString(), &offset);
					if(!symname.isEmpty()) {
						ret << QString("%1 = %2 <%3>").arg(temp_operand, edb::v1::format_pointer(effective_address), symname);

						if(offset == 0) {
							if(is_call(inst)) {
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
				} while(0);
				break;
			case edb::Operand::TYPE_REGISTER:
				do {
					int offset;
					const QString symname = edb::v1::find_function_symbol(effective_address, QString(), &offset);
					if(!symname.isEmpty()) {
						ret << QString("%1 = %2 <%3>").arg(temp_operand, edb::v1::format_pointer(effective_address), symname);

						if(offset == 0) {
							if(is_call(inst)) {
								resolve_function_parameters(state, symname, 0, ret);
							} else {
								resolve_function_parameters(state, symname, 4, ret);
							}
						}

					} else {
						ret << QString("%1 = %2").arg(temp_operand, edb::v1::format_pointer(effective_address));
					}
				} while(0);
				break;

			case edb::Operand::TYPE_EXPRESSION:
			default:
				do {
					edb::address_t target(0);

					if(process->read_bytes(effective_address, &target, edb::v1::pointer_size())) {
						int offset;
						const QString symname = edb::v1::find_function_symbol(target, QString(), &offset);
						if(!symname.isEmpty()) {
							ret << QString("%1 = [%2] = %3 <%4>").arg(temp_operand, edb::v1::format_pointer(effective_address), edb::v1::format_pointer(target), symname);

							if(offset == 0) {
								if(is_call(inst)) {
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
				} while(0);
				break;
			}
		}
	}
}

bool isFPU_BCD(edb::Operand const& operand) {

	const auto op=operand.owner()->operation();
	return op==edb::Instruction::Operation::X86_INS_FBLD ||
		   op==edb::Instruction::Operation::X86_INS_FBSTP;
}

QString formatBCD(edb::value80 const& v) {

	auto hex=v.toHexString();
	// Low bytes which contain 18 digits must be decimal. If not, return the raw hex value.
	if(hex.mid(2).contains(QRegExp("[A-Fa-f]")))
		return "0x"+hex;
	hex.replace(QRegExp("^..0*"),"");
	return (v.negative() ? '-'+hex : hex)+" (BCD)";
}

template<typename ValueType>
QString formatPackedFloat(const char* data,std::size_t size) {

	QString str;
	for(std::size_t offset=0;offset<size;offset+=sizeof(ValueType)) {

		ValueType value;
		std::memcpy(&value,data+offset,sizeof value);
		if(!str.isEmpty()) str+=", ";
		str+=formatFloat(value);
	}
	return size==sizeof(ValueType) ? str : '{'+str+'}';
}

//------------------------------------------------------------------------------
// Name: analyze_operands
// Desc:
//------------------------------------------------------------------------------
void analyze_operands(const State &state, const edb::Instruction &inst, QStringList &ret) {

	Q_UNUSED(inst);
	
	if(IProcess *process = edb::v1::debugger_core->process()) {
		for(std::size_t j = 0; j < edb::Instruction::MAX_OPERANDS; ++j) {

			const edb::Operand &operand = inst.operands()[j];

			if(operand.valid()) {

				QString temp_operand = QString::fromStdString(edb::v1::formatter().to_string(operand));

				switch(operand.general_type()) {
				case edb::Operand::TYPE_REL:
					do {
#if 0
						bool ok;
						const edb::address_t effective_address = get_effective_address(operand, state,ok);
						if(!ok) return;
						ret << QString("%1 = %2").arg(temp_operand).arg(edb::v1::format_pointer(effective_address));
#endif
					} while(0);
					break;
				case edb::Operand::TYPE_REGISTER:
					{
						Register reg=state[QString::fromStdString(edb::v1::formatter().to_string(operand))];
						QString valueString;
						if(!reg) valueString = ArchProcessor::tr("(Error: obtained invalid register value from State)");
						else {
							if(reg.type()==Register::TYPE_FPU && reg.bitSize()==80)
								valueString=formatFloat(reg.value<edb::value80>());
							else if(operand.is_SIMD_SS())
							{
								valueString=formatFloat(reg.value<edb::value32>());
								temp_operand+="_ss";
							}
							else if(operand.is_SIMD_SD())
							{
								valueString=formatFloat(reg.value<edb::value64>());
								temp_operand+="_sd";
							}
							else if(operand.is_SIMD_PS())
								valueString=formatPackedFloat<edb::value32>(reg.rawData(),reg.bitSize()/8);
							else if(operand.is_SIMD_PD())
								valueString=formatPackedFloat<edb::value64>(reg.rawData(),reg.bitSize()/8);
							else
								valueString = reg.toHexString();
						}
						ret << QString("%1 = %2").arg(temp_operand).arg(valueString);
						break;
					}
				case edb::Operand::TYPE_EXPRESSION:
					{
						bool ok;
						const edb::address_t effective_address = get_effective_address(operand, state,ok);
						if(!ok) continue;
						edb::value256 target;

						if(process->read_bytes(effective_address, &target, sizeof(target))) {
							switch(operand.complete_type()) {
							case edb::Operand::TYPE_EXPRESSION8:
								ret << QString("%1 = [%2] = 0x%3").arg(temp_operand).arg(edb::v1::format_pointer(effective_address)).arg(edb::value8(target).toHexString());
								break;
							case edb::Operand::TYPE_EXPRESSION16:
							{
								const edb::value16 value(target);
								QString valueStr;
								if(inst.is_fpu_taking_integer())
								{
									valueStr=util::formatInt(value,NumberDisplayMode::Signed);
									// FIXME: we have to explicitly say it's decimal because EDB is pretty inconsistent
									// even across values in analysis view about its use of 0x prefix
									// Use of hexadecimal format here is pretty much pointless since the number here is
									// expected to be used in usual numeric computations, not as address or similar
									const std::int16_t signedValue=value;
									if(signedValue>9 || signedValue<-9)
										valueStr+=" (decimal)";
								}
								else
									valueStr="0x"+value.toHexString();
								ret << QString("%1 = [%2] = %3").arg(temp_operand).arg(edb::v1::format_pointer(effective_address)).arg(valueStr);
								break;
							}
							case edb::Operand::TYPE_EXPRESSION32:
							{
								const edb::value32 value(target);
								QString valueStr;
								if(inst.is_fpu_taking_float() || operand.is_SIMD_SS())
									valueStr=formatFloat(value);
								else if(inst.is_fpu_taking_integer())
								{
									valueStr=util::formatInt(value,NumberDisplayMode::Signed);
									// FIXME: we have to explicitly say it's decimal because EDB is pretty inconsistent
									// even across values in analysis view about its use of 0x prefix
									// Use of hexadecimal format here is pretty much pointless since the number here is
									// expected to be used in usual numeric computations, not as address or similar
									const std::int32_t signedValue=value;
									if(signedValue>9 || signedValue<-9)
										valueStr+=" (decimal)";
								}
								else if(operand.is_SIMD_PS())
									valueStr=formatPackedFloat<edb::value32>(reinterpret_cast<const char*>(&target),sizeof(edb::value64));
								else if(operand.is_SIMD_PD())
									valueStr=formatPackedFloat<edb::value64>(reinterpret_cast<const char*>(&target),sizeof(edb::value64));
								else
									valueStr="0x"+value.toHexString();
								ret << QString("%1 = [%2] = %3").arg(temp_operand).arg(edb::v1::format_pointer(effective_address)).arg(valueStr);
								break;
							}
							case edb::Operand::TYPE_EXPRESSION64:
							{
								const edb::value64 value(target);
								QString valueStr;
								if(inst.is_fpu_taking_float() || operand.is_SIMD_SS())
									valueStr=formatFloat(value);
								else if(inst.is_fpu_taking_integer())
								{
									valueStr=util::formatInt(value,NumberDisplayMode::Signed);
									// FIXME: we have to explicitly say it's decimal because EDB is pretty inconsistent
									// even across values in analysis view about its use of 0x prefix
									// Use of hexadecimal format here is pretty much pointless since the number here is
									// expected to be used in usual numeric computations, not as address or similar
									const std::int64_t signedValue=value;
									if(signedValue>9 || signedValue<-9)
										valueStr+=" (decimal)";
								}
								else
									valueStr="0x"+value.toHexString();
								ret << QString("%1 = [%2] = %3").arg(temp_operand).arg(edb::v1::format_pointer(effective_address)).arg(valueStr);
								break;
							}
							case edb::Operand::TYPE_EXPRESSION80:
							{
								const edb::value80 value(target);
								const QString valueStr = inst.is_fpu() ? isFPU_BCD(operand) ? formatBCD(value) : formatFloat(value) : "0x"+value.toHexString();
								ret << QString("%1 = [%2] = %3").arg(temp_operand).arg(edb::v1::format_pointer(effective_address)).arg(valueStr);
								break;
							}
							case edb::Operand::TYPE_EXPRESSION128:
							{
								QString valueString;
								if(operand.is_SIMD_PS())
									valueString=formatPackedFloat<edb::value32>(reinterpret_cast<const char*>(&target),sizeof(edb::value128));
								else if(operand.is_SIMD_PD())
									valueString=formatPackedFloat<edb::value64>(reinterpret_cast<const char*>(&target),sizeof(edb::value128));
								else
									valueString="0x"+edb::value128(target).toHexString();
								ret << QString("%1 = [%2] = %3").arg(temp_operand).arg(edb::v1::format_pointer(effective_address)).arg(valueString);
								break;
							}
							case edb::Operand::TYPE_EXPRESSION256:
							{
								QString valueString;
								if(operand.is_SIMD_PS())
									valueString=formatPackedFloat<edb::value32>(reinterpret_cast<const char*>(&target),sizeof(edb::value256));
								else if(operand.is_SIMD_PD())
									valueString=formatPackedFloat<edb::value64>(reinterpret_cast<const char*>(&target),sizeof(edb::value256));
								else
									valueString="0x"+edb::value256(target).toHexString();
								ret << QString("%1 = [%2] = %3").arg(temp_operand).arg(edb::v1::format_pointer(effective_address)).arg(valueString);
								break;
							}
							default:
								ret << QString("%1 = [%2] = 0x%3").arg(temp_operand).arg(edb::v1::format_pointer(effective_address))
																  .arg(QString("<Error: unexpected size; low bytes form %2>").arg(target.toHexString()));
								break;
							}
						} else {
							ret << QString("%1 = [%2] = ?").arg(temp_operand).arg(edb::v1::format_pointer(effective_address));
						}
					}
					break;
				default:
					break;
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

	quint8 buffer[edb::Instruction::MAX_SIZE];

	for(edb::address_t addr = start_address; addr < end_address; ++addr) {
		if(const int sz = edb::v1::get_instruction_bytes(addr, buffer)) {
			edb::Instruction inst(buffer, buffer + sz, addr);
			if(is_jump(inst)) {
				const edb::Operand &operand = inst.operands()[0];

				if(operand.general_type() == edb::Operand::TYPE_REL) {
					const edb::address_t target = operand.relative_target();

					if(target == address) {
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
	Q_UNUSED(inst);
	Q_UNUSED(ret);
	Q_UNUSED(state);

#ifdef Q_OS_LINUX
	QString syscall_entry;
	QDomDocument syscall_xml;
	QFile file(":/debugger/xml/syscalls.xml");
	if(file.open(QIODevice::ReadOnly | QIODevice::Text)) {

		QXmlQuery query;
		QString res;
		query.setFocus(&file);
		const QString arch=debuggeeIs64Bit() ? "x86-64" : "x86";
		query.setQuery(QString("syscalls[@version='1.0']/linux[@arch='"+arch+"']/syscall[index=%1]").arg(regAX));
		if (query.isValid()) {
			query.evaluateTo(&syscall_entry);
		}
		file.close();
	}

	if(!syscall_entry.isEmpty()) {
		syscall_xml.setContent("" + syscall_entry + "");
		QDomElement root = syscall_xml.documentElement();

		QStringList arguments;

		for (QDomElement argument = root.firstChildElement("argument"); !argument.isNull(); argument = argument.nextSiblingElement("argument")) {
			const QString argument_type     = argument.attribute("type");
			const QString argument_register = argument.attribute("register");
			arguments << format_argument(argument_type, state[argument_register]);
		}

		ret << ArchProcessor::tr("SYSCALL: %1(%2)").arg(root.attribute("name"), arguments.join(","));
	}
#endif
}

}

//------------------------------------------------------------------------------
// Name: ArchProcessor
// Desc:
//------------------------------------------------------------------------------
ArchProcessor::ArchProcessor() {
	if(edb::v1::debugger_core) {
		has_mmx_ = edb::v1::debugger_core->has_extension(edb::string_hash("MMX"));
		has_xmm_ = edb::v1::debugger_core->has_extension(edb::string_hash("XMM"));
		has_ymm_ = edb::v1::debugger_core->has_extension(edb::string_hash("YMM"));
		connect(edb::v1::debugger_ui, SIGNAL(attachEvent()), this, SLOT(just_attached()));
	} else {
		has_mmx_ = false;
		has_xmm_ = false;
		has_ymm_ = false;
	}

}

//------------------------------------------------------------------------------
// Name: setup_register_view
// Desc:
//------------------------------------------------------------------------------
void ArchProcessor::setup_register_view() {

	if(edb::v1::debugger_core) {

		update_register_view(QString(), State());
	}
}

//------------------------------------------------------------------------------
// Name: reset
// Desc:
//------------------------------------------------------------------------------
void ArchProcessor::reset() {

	if(edb::v1::debugger_core) {
		update_register_view(QString(), State());
	}
}

QString gprComment(const Register& reg)
{
	QString regString;
	int stringLength;
	QString comment;
	if(edb::v1::get_ascii_string_at_address(reg.valueAsAddress(), regString, edb::v1::config().min_string_length, 256, stringLength))
		comment=QString("ASCII \"%1\"").arg(regString);
	else if(edb::v1::get_utf16_string_at_address(reg.valueAsAddress(), regString, edb::v1::config().min_string_length, 256, stringLength))
		comment=QString("UTF16 \"%1\"").arg(regString);
	return comment;
}

RegisterViewModel& getModel() {
	return static_cast<RegisterViewModel&>(edb::v1::arch_processor().get_register_view_model());
}

void updateGPRs(RegisterViewModel& model, const State& state, bool is64Bit) {
	if(is64Bit) {
		for(std::size_t i=0;i<GPR64_COUNT;++i) {
			const auto reg=state.gp_register(i);
			Q_ASSERT(!!reg); Q_ASSERT(reg.bitSize()==64);
			QString comment;
			if(i==0) {
				const auto origAX=state["orig_rax"].valueAsSignedInteger();
				if(origAX!=-1)
				{
					comment="orig: "+edb::value64(origAX).toHexString();
					const auto errName=syscallErrName(reg.value<edb::value64>());
					if(!errName.isEmpty())
						comment="-"+errName+"; "+comment;
				}
			}
			if(comment.isEmpty())
				comment=gprComment(reg);
			model.updateGPR(i,reg.value<edb::value64>(),comment);
		}
	} else {
		for(std::size_t i=0;i<GPR32_COUNT;++i) {
			const auto reg=state.gp_register(i);
			Q_ASSERT(!!reg); Q_ASSERT(reg.bitSize()==32);
			QString comment;
			if(i==0) {
				const auto origAX=state["orig_eax"].valueAsSignedInteger();
				if(origAX!=-1)
				{
					comment="orig: "+edb::value32(origAX).toHexString();
					const auto errName=syscallErrName(reg.value<edb::value32>());
					if(!errName.isEmpty())
						comment="-"+errName+"; "+comment;
				}
			}
			if(comment.isEmpty())
				comment=gprComment(reg);
			model.updateGPR(i,reg.value<edb::value32>(),comment);
		}
	}
}

QString rIPcomment(edb::address_t rIP, const QString &default_region_name) {
	const auto symname=edb::v1::find_function_symbol(rIP, default_region_name);
	return symname.isEmpty() ? symname : '<'+symname+'>';
}

QString eflagsComment(edb::reg_t flags) {
	QString comment="(";
	for(int cond=0;cond<0x10;++cond)
		if(is_jcc_taken(flags,static_cast<edb::Instruction::ConditionCode>(cond)))
			comment+=jumpConditionMnemonics[cond]+',';
	comment[comment.size()-1]=')';
	return comment;
}

void updateGeneralStatusRegs(RegisterViewModel& model, const State& state, bool is64Bit, const QString &default_region_name) {
	const auto ip=state.instruction_pointer_register();
	const auto flags=state.flags_register();
	Q_ASSERT(!!ip);
	Q_ASSERT(!!flags);
	const auto ipComment=rIPcomment(ip.valueAsAddress(),default_region_name);
	const auto flagsComment=eflagsComment(flags.valueAsInteger());
	if(is64Bit) {
		model.updateIP(ip.value<edb::value64>(),ipComment);
		model.updateFlags(flags.value<edb::value64>(),flagsComment);
	} else {
		model.updateIP(ip.value<edb::value32>(),ipComment);
		model.updateFlags(flags.value<edb::value32>(),flagsComment);
	}
}

QString FPUStackFaultDetail(uint16_t statusWord) {
	const bool invalidOperationException=statusWord & 0x01;
	const bool C1=statusWord&(1<<9);
	const bool stackFault=statusWord&0x40;
	if(invalidOperationException && stackFault)
		return C1 ? QObject::tr("Stack overflow") : QObject::tr("Stack underflow");
	return "";
}

QString FPUComparExplain(uint16_t statusWord) {
	const bool C0=statusWord&(1<<8);
	const bool C2=statusWord&(1<<10);
	const bool C3=statusWord&(1<<14);
	if(C3==0 && C2==0 && C0==0) return "GT";
	if(C3==0 && C2==0 && C0==1) return "LT";
	if(C3==1 && C2==0 && C0==0) return "EQ";
	if(C3==1 && C2==1 && C0==1) return QObject::tr("Unordered","result of FPU comparison instruction");
	return "";
}

QString FPUExplainPE(uint16_t statusWord) {
	if(statusWord&(1<<5)) {
		const bool C1=statusWord&(1<<9);
		return C1 ? QObject::tr("Rounded UP") : QObject::tr("Rounded DOWN");
	}
	return "";
}

QString FSRComment(uint16_t statusWord) {

	const auto stackFaultDetail=FPUStackFaultDetail(statusWord);
	const auto comparisonResult=FPUComparExplain(statusWord);
	const auto comparComment=comparisonResult.isEmpty()?"":'('+comparisonResult+')';
	const auto peExplanation=FPUExplainPE(statusWord);

	auto comment=comparComment;
	if(comment.length() && stackFaultDetail.length()) comment+=", ";
	comment+=stackFaultDetail;
	if(comment.length() && peExplanation.length()) comment+=", ";
	comment+=peExplanation;
	return comment.trimmed();
}

void updateSegRegs(RegisterViewModel& model, const State& state) {
	static const QString sregs[]={"es","cs","ss","ds","fs","gs"};
    for(std::size_t i=0;i<sizeof(sregs)/sizeof(sregs[0]);++i) {
        QString sreg(sregs[i]);
        const auto sregValue=state[sreg].value<edb::seg_reg_t>();
		const Register base=state[sregs[i]+"_base"];
		QString comment;
		if(edb::v1::debuggeeIs32Bit() || i>=FS) {
			if(base)
				comment=QString("(%1)").arg(base.valueAsAddress().toHexString());
			else if(edb::v1::debuggeeIs32Bit() && sregValue==0)
				comment="NULL";
			else
				comment="(?)";
		}
		model.updateSegReg(i,sregValue,comment);
	}
}

void updateFPURegs(RegisterViewModel& model, const State& state) {
	for(std::size_t i=0;i<MAX_FPU_REGS_COUNT;++i)
	{
		const auto reg=state.fpu_register(i);
		const auto comment = floatType(reg)==FloatValueClass::PseudoDenormal ?
								QObject::tr("pseudo-denormal") : "";
		model.updateFPUReg(i,reg,comment);
	}
	model.updateFCR(state.fpu_control_word());
	const auto fsr=state.fpu_status_word();
	model.updateFSR(fsr,FSRComment(fsr));
	model.updateFTR(state.fpu_tag_word());
	{
		const Register FIS=state["FIS"];
		if(FIS) model.updateFIS(FIS.value<edb::value16>());
		else model.invalidateFIS();
	}
	{
		const Register FDS=state["FDS"];
		if(FDS) model.updateFDS(FDS.value<edb::value16>());
		else model.invalidateFDS();
	}
	{
		const Register FIP=state["FIP"];
		if(FIP.bitSize()==64)
			model.updateFIP(FIP.value<edb::value64>());
		else if(FIP.bitSize()==32)
			model.updateFIP(FIP.value<edb::value32>());
		else
			model.invalidateFIP();
	}
	{
		const Register FDP=state["FDP"];
		if(FDP.bitSize()==64)
			model.updateFDP(FDP.value<edb::value64>());
		else if(FDP.bitSize()==32)
			model.updateFDP(FDP.value<edb::value32>());
		else
			model.invalidateFDP();
	}
	{
		const Register FOP=state["fopcode"];
		if(FOP) {
			const auto value=FOP.value<edb::value16>();
			// Yes, FOP is a big-endian view of the instruction
			const auto comment=value>0x7ff ? QString("?!!") :
								QObject::tr("Insn: %1 %2").arg((edb::value8(value>>8)+0xd8).toHexString()).arg(edb::value8(value).toHexString());
			model.updateFOP(value,comment);
		}
		else model.invalidateFOP();
	}
}

void updateDebugRegs(RegisterViewModel& model, const State& state) {
	for(std::size_t i=0;i<MAX_DEBUG_REGS_COUNT;++i) {
		const edb::reg_t reg=state.debug_register(i);
		if(edb::v1::debuggeeIs32Bit())
			model.updateDR(i,edb::value32(reg));
		else model.updateDR(i,reg);
	}
}

void updateMMXRegs(RegisterViewModel& model, const State& state) {
	for(std::size_t i=0;i<MAX_MMX_REGS_COUNT;++i) {
		const auto reg=state.mmx_register(i);
		if(!!reg) model.updateMMXReg(i,reg.value<MMWord>());
		else model.invalidateMMXReg(i);
	}
}

void updateSSEAVXRegs(RegisterViewModel& model, const State& state, bool hasSSE, bool hasAVX) {
	if(!hasSSE) return;
	const std::size_t max=edb::v1::debuggeeIs32Bit() ? AVX32_COUNT : AVX64_COUNT;
	for(std::size_t i=0;i<max;++i) {
		if(hasAVX) {
			const auto reg=state.ymm_register(i);
			if(!reg) model.invalidateAVXReg(i);
			else model.updateAVXReg(i,reg.value<YMMWord>());
		} else if(hasSSE) {
			const auto reg=state.xmm_register(i);
			if(!reg) model.invalidateSSEReg(i);
			else model.updateSSEReg(i,reg.value<XMMWord>());
		}
	}
	const auto mxcsr=state["mxcsr"];
	if(!mxcsr) model.invalidateMXCSR();
	else model.updateMXCSR(mxcsr.value<edb::value32>());
}

//------------------------------------------------------------------------------
// Name: update_register_view
// Desc:
//------------------------------------------------------------------------------
void ArchProcessor::update_register_view(const QString &default_region_name, const State &state) {
	const QPalette palette = QApplication::palette();

	auto& model=getModel();

	const auto ip=state.instruction_pointer_register();

	if(!ip) {
		model.setCPUMode(RegisterViewModel::CPUMode::UNKNOWN);
		return;
	}
	// FIXME: this function will crash for 32-bit process, jumped to 64-bit segment, if EDB
	// doesn't find a way to get full 64-bit state for such process
	const bool is64Bit=ip.bitSize()==64;
	Q_ASSERT(is64Bit || ip.bitSize()==32);
	
	model.setCPUMode(is64Bit ? RegisterViewModel::CPUMode::AMD64 : RegisterViewModel::CPUMode::IA32);
	updateGPRs(model,state,is64Bit);
	updateGeneralStatusRegs(model,state,is64Bit,default_region_name);
	updateSegRegs(model,state);
	updateFPURegs(model,state);
	updateDebugRegs(model,state);
	updateMMXRegs(model,state);
	updateSSEAVXRegs(model,state,has_xmm_,has_ymm_);

	if(just_attached_) {
		model.saveValues();
		just_attached_=false;
	}
	model.dataUpdateFinished();
}

void ArchProcessor::about_to_resume() {
	getModel().saveValues();
}

void ArchProcessor::just_attached() {
	just_attached_=true;
}

bool falseSyscallReturn(State const& state, std::int64_t origAX) {
	// Prevent reporting of returns from execve() when the process has just launched
	if(EDB_IS_32_BIT && origAX==11) {
		return state.gp_register(rAX).valueAsInteger()==0 &&
			   state.gp_register(rCX).valueAsInteger()==0 &&
			   state.gp_register(rDX).valueAsInteger()==0 &&
			   state.gp_register(rBX).valueAsInteger()==0 &&
			   state.gp_register(rBP).valueAsInteger()==0 &&
			   state.gp_register(rSI).valueAsInteger()==0 &&
			   state.gp_register(rDI).valueAsInteger()==0;
	}
	else if(EDB_IS_64_BIT && origAX==59) {
		return state.gp_register(rAX).valueAsInteger()==0 &&
			   state.gp_register(rCX).valueAsInteger()==0 &&
			   state.gp_register(rDX).valueAsInteger()==0 &&
			   state.gp_register(rBX).valueAsInteger()==0 &&
			   state.gp_register(rBP).valueAsInteger()==0 &&
			   state.gp_register(rSI).valueAsInteger()==0 &&
			   state.gp_register(rDI).valueAsInteger()==0 &&
			   state.gp_register(R8 ).valueAsInteger()==0 &&
			   state.gp_register(R9 ).valueAsInteger()==0 &&
			   state.gp_register(R10).valueAsInteger()==0 &&
			   state.gp_register(R11).valueAsInteger()==0 &&
			   state.gp_register(R12).valueAsInteger()==0 &&
			   state.gp_register(R13).valueAsInteger()==0 &&
			   state.gp_register(R14).valueAsInteger()==0 &&
			   state.gp_register(R15).valueAsInteger()==0;
	}
	return false;
}

//------------------------------------------------------------------------------
// Name: update_instruction_info
// Desc:
//------------------------------------------------------------------------------
QStringList ArchProcessor::update_instruction_info(edb::address_t address) {

	QStringList ret;

	Q_ASSERT(edb::v1::debugger_core);

	if(IProcess *process = edb::v1::debugger_core->process()) {
		quint8 buffer[edb::Instruction::MAX_SIZE];

		if(process->read_bytes(address, buffer, sizeof(buffer))) {
			edb::Instruction inst(buffer, buffer + sizeof(buffer), address);
			if(inst) {

				State state;
				edb::v1::debugger_core->get_state(&state);

				std::int64_t origAX;
				if(debuggeeIs64Bit())
					origAX=state["orig_rax"].valueAsSignedInteger();
				else
					origAX=state["orig_eax"].valueAsSignedInteger();
				const std::uint64_t rax=state.gp_register(rAX).valueAsSignedInteger();
				if(origAX!=-1 && !falseSyscallReturn(state,origAX)) {
					// FIXME: this all doesn't work correctly when we're on the first instruction of a signal handler
					// The registers there don't correspond to arguments of the syscall, and it's not correct to say the
					// debuggee _returned_ from the syscall, since it's just interrupted the syscall to handle the signal
					analyze_syscall(state, inst, ret, origAX);
#ifdef Q_OS_LINUX
					enum {
						// restart if no handler or if SA_RESTART is set, can be seen when interrupting e.g. waitpid
						ERESTARTSYS=512,
						// restart unconditionally
						ERESTARTNOINTR=513,
						// restart if no handler
						ERESTARTNOHAND=514,
						// restart by sys_restart_syscall, can be seen when interrupting e.g. nanosleep
						ERESTART_RESTARTBLOCK=516,
					};
					const auto err = rax>=-4095UL ? -rax : 0;
					const bool interrupted = err==EINTR ||
											 err==ERESTARTSYS ||
											 err==ERESTARTNOINTR ||
											 err==ERESTARTNOHAND ||
											 err==ERESTART_RESTARTBLOCK;

					if(ret.size() && ret.back().startsWith("SYSCALL")) {
						if(interrupted)
							ret.back()="Interrupted "+ret.back();
						else
							ret.back()="Returned from "+ret.back();
					}
					// FIXME: actually only ERESTARTNOINTR guarantees reexecution. But it seems the other ERESTART* signals
					// won't go into user space, so whatever the state of signal handlers, the tracee should never appear
					// to see these signals. So I guess it's OK to assume that tha syscall _will_ be restarted by the kernel.
					if(interrupted && err!=EINTR)
						ret << QString("Syscall will be restarted on next step/run");
#endif
				}

				// figure out the instruction type and display some information about it
				// TODO: handle SETcc, LOOPcc, REPcc OP
				if(inst.is_conditional_move()) {

					analyze_cmov(state, inst, ret);

				} else if(inst.is_ret()) {

					analyze_return(state, inst, ret);

				} else if(inst.is_jump() || inst.is_call()) {

					if(is_conditional_jump(inst))
						analyze_jump(state, inst, ret);
					analyze_call(state, inst, ret);
				} else if(inst.is_int()) {
				#ifdef Q_OS_LINUX
				   if((inst.operands()[0].immediate() & 0xff) == 0x80) {

						analyze_syscall(state, inst, ret, state.gp_register(rAX).valueAsInteger());
					} else {

						analyze_operands(state, inst, ret);
					}
				#endif
				} else if (inst.is_syscall() || inst.is_sysenter()) {

					analyze_syscall(state, inst, ret, state.gp_register(rAX).valueAsInteger());

				} else {

					analyze_operands(state, inst, ret);
				}

				analyze_jump_targets(inst, ret);

			}
		}

		// eliminate duplicates
		ret = QStringList::fromSet(ret.toSet());
		// elimination of duplicates left ret in a strange order, make it easier to follow
		ret.sort();
	}
	return ret;
}

//------------------------------------------------------------------------------
// Name: can_step_over
// Desc:
//------------------------------------------------------------------------------
bool ArchProcessor::can_step_over(const edb::Instruction &inst) const {
	return inst && (is_call(inst) || (inst.prefix() & (edb::Instruction::PREFIX_REPNE | edb::Instruction::PREFIX_REP)));
}

//------------------------------------------------------------------------------
// Name: is_filling
// Desc:
//------------------------------------------------------------------------------
bool ArchProcessor::is_filling(const edb::Instruction &inst) const {
	bool ret = false;

	// fetch the operands
	if(inst) {
		const edb::Operand operands[edb::Instruction::MAX_OPERANDS] = {
			inst.operands()[0],
			inst.operands()[1],
			inst.operands()[2]
		};

		switch(inst.operation()) {
		case edb::Instruction::Operation::X86_INS_NOP:
		case edb::Instruction::Operation::X86_INS_INT3:
			ret = true;
			break;

		case edb::Instruction::Operation::X86_INS_LEA:
			if(operands[0].valid() && operands[1].valid()) {
				if(operands[0].general_type() == edb::Operand::TYPE_REGISTER && operands[1].general_type() == edb::Operand::TYPE_EXPRESSION) {

					edb::Operand::Register reg1;
					edb::Operand::Register reg2;

					reg1 = operands[0].reg();

					if(operands[1].expression().scale == 1) {
						if(operands[1].expression().displacement == 0) {

							if(operands[1].expression().base == edb::Operand::Register::X86_REG_INVALID) {
								reg2 = operands[1].expression().index;
								ret = reg1 == reg2;
							} else if(operands[1].expression().index == edb::Operand::Register::X86_REG_INVALID) {
								reg2 = operands[1].expression().base;
								ret = reg1 == reg2;
							}
						}
					}
				}
			}
			break;

		case edb::Instruction::Operation::X86_INS_MOV:
			if(operands[0].valid() && operands[1].valid()) {
				if(operands[0].general_type() == edb::Operand::TYPE_REGISTER && operands[1].general_type() == edb::Operand::TYPE_REGISTER) {
					ret = operands[0].reg() == operands[1].reg();
				}
			}
			break;

		default:
			break;
		}

		if(!ret) {
			if(edb::v1::config().zeros_are_filling) {
				ret = (QByteArray::fromRawData(reinterpret_cast<const char *>(inst.bytes()), inst.size()) == QByteArray::fromRawData("\x00\x00", 2));
			}
		}
	} else {
		ret = (inst.size() == 1 && inst.bytes()[0] == 0x00);
	}

	return ret;
}

//------------------------------------------------------------------------------
// Name: register_view_model
// Desc:
//------------------------------------------------------------------------------
RegisterViewModelBase::Model& ArchProcessor::get_register_view_model() const {
    static RegisterViewModel model(has_mmx_*RegisterViewModel::CPUFeatureBits::MMX |
								   has_xmm_*RegisterViewModel::CPUFeatureBits::SSE |
								   has_ymm_*RegisterViewModel::CPUFeatureBits::AVX);
    return model;
}
