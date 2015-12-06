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

#include "DialogEditGPR.h"
#include "DialogEditFPU.h"
#include "FloatX.h"
#include "DialogEditSIMDRegister.h"

#ifdef Q_OS_LINUX
#include <asm/unistd.h>
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
static constexpr size_t MAX_GPR_COUNT=16;
static constexpr size_t MAX_FPU_REGS_COUNT=8;
static constexpr size_t MAX_MMX_REGS_COUNT=MAX_FPU_REGS_COUNT;
static constexpr size_t MAX_XMM_REGS_COUNT=MAX_GPR_COUNT;
static constexpr size_t MAX_YMM_REGS_COUNT=MAX_GPR_COUNT;
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

				if(!!baseR)
					base=baseR.valueAsAddress();
				if(!!indexR)
					index=indexR.valueAsAddress();

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
bool is_jcc_taken(const State &state, edb::Instruction::ConditionCode cond) {

	if(cond==edb::Instruction::CC_UNCONDITIONAL) return true;
	if(cond==edb::Instruction::CC_RCXZ) return state.gp_register(rCX).value<edb::value64>() == 0;
	if(cond==edb::Instruction::CC_ECXZ) return state.gp_register(rCX).value<edb::value32>() == 0;
	if(cond==edb::Instruction::CC_CXZ)  return state.gp_register(rCX).value<edb::value16>() == 0;

	const edb::reg_t efl = state.flags();
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

				const QString temp_operand = QString::fromStdString(edb::v1::formatter().to_string(operand));

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
							else
								valueString = reg.toHexString();
						}
						ret << QString("%1 = %2").arg(temp_operand).arg(valueString);
						break;
					}
				case edb::Operand::TYPE_EXPRESSION:
					do {
						bool ok;
						const edb::address_t effective_address = get_effective_address(operand, state,ok);
						if(!ok) return;
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
									// FIXME: we have to explicitly say it's decimal because EDB is pretty inconsistent
									// even across values in analysis view about its use of 0x prefix
									// Use of hexadecimal format here is pretty much pointless since the number here is
									// expected to be used in usual numeric computations, not as address or similar
									valueStr=util::formatInt(value,IntDisplayMode::Signed)+" (decimal)";
								else
									valueStr="0x"+value.toHexString();
								ret << QString("%1 = [%2] = %3").arg(temp_operand).arg(edb::v1::format_pointer(effective_address)).arg(valueStr);
								break;
							}
							case edb::Operand::TYPE_EXPRESSION32:
							{
								const edb::value32 value(target);
								QString valueStr;
								if(inst.is_fpu_taking_float())
									valueStr=formatFloat(value);
								else if(inst.is_fpu_taking_integer())
									// FIXME: we have to explicitly say it's decimal because EDB is pretty inconsistent
									// even across values in analysis view about its use of 0x prefix
									// Use of hexadecimal format here is pretty much pointless since the number here is
									// expected to be used in usual numeric computations, not as address or similar
									valueStr=util::formatInt(value,IntDisplayMode::Signed)+" (decimal)";
								else
									valueStr="0x"+value.toHexString();
								ret << QString("%1 = [%2] = %3").arg(temp_operand).arg(edb::v1::format_pointer(effective_address)).arg(valueStr);
								break;
							}
							case edb::Operand::TYPE_EXPRESSION64:
							{
								const edb::value64 value(target);
								QString valueStr;
								if(inst.is_fpu_taking_float())
									valueStr=formatFloat(value);
								else if(inst.is_fpu_taking_integer())
									// FIXME: we have to explicitly say it's decimal because EDB is pretty inconsistent
									// even across values in analysis view about its use of 0x prefix
									// Use of hexadecimal format here is pretty much pointless since the number here is
									// expected to be used in usual numeric computations, not as address or similar
									valueStr=util::formatInt(value,IntDisplayMode::Signed)+" (decimal)";
								else
									valueStr="0x"+value.toHexString();
								ret << QString("%1 = [%2] = %3").arg(temp_operand).arg(edb::v1::format_pointer(effective_address)).arg(valueStr);
								break;
							}
							case edb::Operand::TYPE_EXPRESSION80:
							{
								const edb::value80 value(target);
								const QString valueStr = inst.is_fpu() ? formatFloat(value) : "0x"+value.toHexString();
								ret << QString("%1 = [%2] = %3").arg(temp_operand).arg(edb::v1::format_pointer(effective_address)).arg(valueStr);
								break;
							}
							case edb::Operand::TYPE_EXPRESSION128:
								ret << QString("%1 = [%2] = 0x%3").arg(temp_operand).arg(edb::v1::format_pointer(effective_address)).arg(edb::value128(target).toHexString());
								break;
							case edb::Operand::TYPE_EXPRESSION256:
								ret << QString("%1 = [%2] = 0x%3").arg(temp_operand).arg(edb::v1::format_pointer(effective_address)).arg(edb::value256(target).toHexString());
								break;
							default:
								ret << QString("%1 = [%2] = 0x%3").arg(temp_operand).arg(edb::v1::format_pointer(effective_address))
																  .arg(QString("<Error: unexpected size; low bytes form %2>").arg(target.toHexString()));
								break;
							}
						} else {
							ret << QString("%1 = [%2] = ?").arg(temp_operand).arg(edb::v1::format_pointer(effective_address));
						}
					} while(0);
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
void ArchProcessor::setup_register_view(RegisterListWidget *category_list) {

	if(edb::v1::debugger_core) {
		State state;

		Q_ASSERT(category_list);

		update_register_view(QString(), State());
	}
}

//------------------------------------------------------------------------------
// Name: reset
// Desc:
//------------------------------------------------------------------------------
void ArchProcessor::reset() {

	if(edb::v1::debugger_core) {
		last_state_.clear();
		update_register_view(QString(), State());
	}
}

QString getFPUStatus(uint16_t statusWord) {
	const bool fpuBusy=statusWord&0x8000;
	QString fpuBusyString;
	if(fpuBusy)
		fpuBusyString=" BUSY";
	QString stackFaultDetail;
	const int exceptionsHappened=statusWord&0x3f;
	const bool invalidOperationException=(exceptionsHappened & 0x01);
	const bool C1=statusWord&(1<<9);
	const bool stackFault=statusWord&0x40;
	if(invalidOperationException && stackFault) {
		stackFaultDetail=C1?" Stack overflow":" Stack underflow";
	}
	QString fpuStatus=fpuBusyString;
	if(stackFaultDetail.size())
		fpuStatus += (fpuStatus.size() ? "," : "")+stackFaultDetail;
	return fpuStatus.isEmpty() ? " OK" : fpuStatus;
}

QString ArchProcessor::getRoundingMode(unsigned modeBits) const {
	switch(modeBits) {
	case 0: return tr("Rounding to nearest");
	case 1: return tr("Rounding down");
	case 2: return tr("Rounding up");
	case 3: return tr("Rounding toward zero");
	}
	return "???";
}

QString getExceptionMaskString(unsigned exceptionMask) {
	QString exceptionMaskString;
	exceptionMaskString += ((exceptionMask & 0x01) ? " IM" : " Iu");
	exceptionMaskString += ((exceptionMask & 0x02) ? " DM" : " Du");
	exceptionMaskString += ((exceptionMask & 0x04) ? " ZM" : " Zu");
	exceptionMaskString += ((exceptionMask & 0x08) ? " OM" : " Ou");
	exceptionMaskString += ((exceptionMask & 0x10) ? " UM" : " Uu");
	exceptionMaskString += ((exceptionMask & 0x20) ? " PM" : " Pu");
	return exceptionMaskString;
}

QString getActiveExceptionsString(unsigned exceptionsHappened) {
	QString exceptionsHappenedString;
	exceptionsHappenedString += ((exceptionsHappened & 0x01) ? " IE" : "");
	exceptionsHappenedString += ((exceptionsHappened & 0x02) ? " DE" : "");
	exceptionsHappenedString += ((exceptionsHappened & 0x04) ? " ZE" : "");
	exceptionsHappenedString += ((exceptionsHappened & 0x08) ? " OE" : "");
	exceptionsHappenedString += ((exceptionsHappened & 0x10) ? " UE" : "");
	exceptionsHappenedString += ((exceptionsHappened & 0x20) ? " PE" : "");
	if(exceptionsHappenedString.isEmpty())
		exceptionsHappenedString=" (none)";
	return exceptionsHappenedString;
}

//------------------------------------------------------------------------------
// Name: update_fpu_view
// Desc:
//------------------------------------------------------------------------------
void ArchProcessor::update_fpu_view(int& itemNumber, const State &state, const QPalette& palette) const {

}

template<typename T>
QString ArchProcessor::formatSIMDRegister(const T& value, SIMDDisplayMode simdMode, IntDisplayMode intMode) {
	QString str;
	switch(simdMode)
	{
	case SIMDDisplayMode::Bytes:
		str=util::packedIntsToString<std::uint8_t>(value,intMode);
		break;
	case SIMDDisplayMode::Words:
		str=util::packedIntsToString<std::uint16_t>(value,intMode);
		break;
	case SIMDDisplayMode::Dwords:
		str=util::packedIntsToString<std::uint32_t>(value,intMode);
		break;
	case SIMDDisplayMode::Qwords:
		str=util::packedIntsToString<std::uint64_t>(value,intMode);
		break;
	case SIMDDisplayMode::Floats32:
		str=util::packedFloatsToString<float>(value);
		break;
	case SIMDDisplayMode::Floats64:
		str=util::packedFloatsToString<double>(value);
		break;
	default:
		str=value.toHexString();
	}
	return str;
}

//------------------------------------------------------------------------------
// Name: update_register_view
// Desc:
//------------------------------------------------------------------------------
void ArchProcessor::update_register_view(const QString &default_region_name, const State &state) {
	const QPalette palette = QApplication::palette();

    // TODO: hide items if state is empty, unhide if non-empty
    // TODO: hide high GPRs if in 32-bit mode
    // XXX: use instruction_pointer() and similar fast functions

	last_state_ = state;
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
				const std::int64_t rax=state.gp_register(rAX).valueAsSignedInteger();
				static const int ERESTARTSYS=512;
				// both EINTR and ERESTARTSYS can be present in any nonzero combination to mean interrupted syscall
				if(origAX!=-1 && (-rax)&(EINTR|ERESTARTSYS)) {
					analyze_syscall(state, inst, ret, origAX);
					if(ret.size() && ret.back().startsWith("SYSCALL"))
						ret.back()="Interrupted "+ret.back();
					if((-rax)&ERESTARTSYS)
						ret << QString("Syscall will be restarted on next step/run");
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

