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
// Name: create_register_item
// Desc:
//------------------------------------------------------------------------------
QTreeWidgetItem *create_register_item(QTreeWidgetItem *parent, const QString &name) {

	auto item = new QTreeWidgetItem(parent);
	item->setData(0, Qt::UserRole, name);
	return item;
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
ArchProcessor::ArchProcessor() : split_flags_(0) {
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

	category_list->clear();
	register_view_items_.clear();

	if(edb::v1::debugger_core) {
		State state;

		Q_ASSERT(category_list);

		// setup the register view
		if(QTreeWidgetItem *const gpr = category_list->addCategory(tr("General Purpose"))) {
			for(std::size_t i=0;i<MAX_GPR_COUNT;++i)
				register_view_items_.push_back(create_register_item(gpr, QString("GPR%1").arg(i)));
			register_view_items_.push_back(create_register_item(gpr, "rIP"));
			register_view_items_.push_back(create_register_item(gpr, "rFLAGS"));

			// split [ER]FLAGS view
			split_flags_ = new QTreeWidgetItem(register_view_items_.back());
			split_flags_->setText(0, state.flags_to_string(0));
		}

		if(QTreeWidgetItem *const segs = category_list->addCategory(tr("Segments"))) {
			for(std::size_t i=0;i<MAX_SEGMENT_REGS_COUNT;++i)
                register_view_items_.push_back(create_register_item(segs, QString("Seg%1").arg(i)));
		}

		if(QTreeWidgetItem *const fpu = category_list->addCategory(tr("FPU"))) {
			for(std::size_t i=0;i<MAX_FPU_REGS_COUNT;++i)
                register_view_items_.push_back(create_register_item(fpu, QString("R%1").arg(i)));
			const auto fcr=create_register_item(fpu, "FCR");
			register_view_items_.push_back(fcr);
			fpu_exceptions_mask_ = new QTreeWidgetItem(fcr);
			fpu_rc_ = new QTreeWidgetItem(fcr);
			fpu_pc_ = new QTreeWidgetItem(fcr);
			const auto fsr=create_register_item(fpu, "FSR");
			register_view_items_.push_back(fsr);
			fpu_exceptions_active_ = new QTreeWidgetItem(fsr);
			fpu_status_ = new QTreeWidgetItem(fsr);
			fpu_top_ = new QTreeWidgetItem(fsr);
			register_view_items_.push_back(create_register_item(fpu, "FTAGS"));
			register_view_items_.push_back(create_register_item(fpu, "FIP"));
			register_view_items_.push_back(create_register_item(fpu, "FDP"));
			register_view_items_.push_back(create_register_item(fpu, "FOPC"));
		}

		if(QTreeWidgetItem *const dbg = category_list->addCategory(tr("Debug"))) {
			for(std::size_t i=0;i<MAX_DEBUG_REGS_COUNT;++i)
                register_view_items_.push_back(create_register_item(dbg, QString("dr%1").arg(i)));
		}

		if(has_mmx_) {
			if(QTreeWidgetItem *const mmx = category_list->addCategory(tr("MMX"))) {
                for(std::size_t i=0;i<MAX_MMX_REGS_COUNT;++i)
                    register_view_items_.push_back(create_register_item(mmx, QString("mm%1").arg(i)));
			}
		}

		QTreeWidgetItem* mxcsr=0;
		if(has_ymm_) {
			if(QTreeWidgetItem *const ymm = category_list->addCategory(tr("AVX"))) {
				for(std::size_t i=0;i<MAX_YMM_REGS_COUNT;++i)
					register_view_items_.push_back(create_register_item(ymm, QString("YMM%1").arg(i)));
				mxcsr=create_register_item(ymm, "mxcsr");
			}
		} else if(has_xmm_) {
			if(QTreeWidgetItem *const xmm = category_list->addCategory(tr("SSE"))) {
				for(std::size_t i=0;i<MAX_XMM_REGS_COUNT;++i)
					register_view_items_.push_back(create_register_item(xmm, QString("XMM%1").arg(i)));
				mxcsr=create_register_item(xmm, "mxcsr");
			}
		}
		if(mxcsr) {
			register_view_items_.push_back(mxcsr);
			mxcsr_rc_ = new QTreeWidgetItem(mxcsr);
			mxcsr_ftz_ = new QTreeWidgetItem(mxcsr);
			mxcsr_daz_ = new QTreeWidgetItem(mxcsr);
			mxcsr_exceptions_mask_ = new QTreeWidgetItem(mxcsr);
			mxcsr_exceptions_active_ = new QTreeWidgetItem(mxcsr);
		}

		update_register_view(QString(), State());
	}
}

//------------------------------------------------------------------------------
// Name: value_from_item
// Desc:
//------------------------------------------------------------------------------
Register ArchProcessor::value_from_item(const QTreeWidgetItem &item) {
	QString preName = item.text(0).split(':').front().trimmed();
	const QString name = preName.mid(0,2)=="=>" ? preName.mid(2) : preName;
	State state;
	edb::v1::debugger_core->get_state(&state);
	return state[name];
}

void ArchProcessor::edit_item(const QTreeWidgetItem &item) {
	if(Register r = value_from_item(item)) {
		if((r.type()==Register::TYPE_GPR ||
		   r.type()==Register::TYPE_SEG ||
		   r.type()==Register::TYPE_IP  ||
		   r.type()==Register::TYPE_COND) && r.bitSize()<=64) {

			static auto gprEdit=new DialogEditGPR(item.treeWidget());
			gprEdit->set_value(r);
			if(gprEdit->exec()==QDialog::Accepted) {
				r=gprEdit->value();
				State state;
				edb::v1::debugger_core->get_state(&state);
				state.set_register(r.name(), r.valueAsInteger());
				edb::v1::debugger_core->set_state(state);
			}
		}
		else if(r.type()==Register::TYPE_FPU) {
			static auto fpuEdit=new DialogEditFPU(item.treeWidget());
			fpuEdit->set_value(r);
			if(fpuEdit->exec()==QDialog::Accepted) {
				State state;
				edb::v1::debugger_core->get_state(&state);
				state.set_register(fpuEdit->value());
				edb::v1::debugger_core->set_state(state);
			}
		}
		else if(r.type()==Register::TYPE_SIMD) {
			static auto simdEdit=new DialogEditSIMDRegister(item.treeWidget());
			simdEdit->set_value(r);
			if(simdEdit->exec()==QDialog::Accepted) {
				State state;
				edb::v1::debugger_core->get_state(&state);
				state.set_register(simdEdit->value());
				edb::v1::debugger_core->set_state(state);
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: update_register
// Desc:
//------------------------------------------------------------------------------
void ArchProcessor::update_register(QTreeWidgetItem *item, const Register &reg) const {

	Q_ASSERT(item);

	item->setHidden(!reg);
	if(!reg) return;

	QString reg_string;
	int string_length;
	const QString name=reg.name().leftJustified(3).toUpper();

	if(edb::v1::get_ascii_string_at_address(reg.valueAsAddress(), reg_string, edb::v1::config().min_string_length, 256, string_length)) {
		item->setText(0, QString("%1: %2 ASCII \"%3\"").arg(name, reg.toHexString(), reg_string));
	} else if(edb::v1::get_utf16_string_at_address(reg.valueAsAddress(), reg_string, edb::v1::config().min_string_length, 256, string_length)) {
		item->setText(0, QString("%1: %2 UTF16 \"%3\"").arg(name, reg.toHexString(), reg_string));
	} else {
		item->setText(0, QString("%1: %2").arg(name, reg.toHexString()));
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

	const int fpuTop=state.fpu_stack_pointer();
	for(int i = 0; i < 8; ++i) {
		const size_t regIndex=fpuOrderMode_==FPUOrderMode::Stack ? (i+fpuTop) % 8 : i;
		const edb::value80 current = state.fpu_register(regIndex);
		const edb::value80 prev    = last_state_.fpu_register(regIndex); // take Rn with the same n so that check for changes makes sense
		bool empty=state.fpu_register_is_empty(regIndex);
		const QString tag=state.fpu_register_tag_string(regIndex);

		bool changed=current != prev;
		QPalette::ColorGroup colGroup(empty ? QPalette::Disabled : QPalette::Normal);
		QBrush textColor(changed ? Qt::red : palette.brush(colGroup,QPalette::Text));
		QString valueStr;
		switch(fpuDisplayMode_)
		{
		case FPUDisplayMode::Float:
			valueStr=formatFloat(current);
			break;
		case FPUDisplayMode::Hex:
		{
			valueStr=current.toHexString();
			const QChar groupSeparator(' ');
			valueStr.insert(valueStr.size()-8,groupSeparator);
			valueStr.insert(valueStr.size()-16-1,groupSeparator);
			break;
		}
		}
		const QString regFormat(fpuOrderMode_==FPUOrderMode::Stack ? QString("ST%1: %2 %3") : QString("%0R%1: %2 %3").arg(fpuTop==i?"=>":"  "));
		register_view_items_[itemNumber]->setText(0, QString(regFormat).arg(i).arg(tag.leftJustified(8)).arg(valueStr));
		register_view_items_[itemNumber++]->setForeground(0, textColor);
	}
	edb::value16 controlWord=state.fpu_control_word();
	int controlWordValue=controlWord;
	edb::value16 statusWord=state.fpu_status_word();
	QString roundingMode=getRoundingMode((controlWordValue>>10)&3);
	QString precisionMode;
	switch((controlWordValue>>8)&3) {
	case 0:
		precisionMode = tr("Single precision (24 bit complete mantissa)");
		break;
	case 1:
		precisionMode = tr("Reserved");
		break;
	case 2:
		precisionMode = tr("Double precision (53 bit complete mantissa)");
		break;
	case 3:
		precisionMode = tr("Extended precision (64 bit mantissa)");
		break;
	}
	int exceptionMask=controlWordValue&0x3f;
	int exceptionsHappened=statusWord&0x3f;
	int prevExMask=last_state_.fpu_control_word()&0x3f;
	int prevExHap=last_state_.fpu_status_word()&0x3f;
	QString exceptionMaskString = getExceptionMaskString(exceptionMask);
	QString exceptionsHappenedString = getActiveExceptionsString(exceptionsHappened);
	QString fpuStatus=getFPUStatus(statusWord);
	register_view_items_[itemNumber]->setText(0, QString("FCR: %1").arg(controlWord.toHexString()));
	register_view_items_[itemNumber++]->setForeground(0, QBrush(controlWord != last_state_.fpu_control_word() ? Qt::red : palette.text()));
	fpu_exceptions_mask_->setText(0, QString("Exceptions mask:%1").arg(exceptionMaskString));
	fpu_exceptions_mask_->setForeground(0, QBrush(exceptionMask != prevExMask ? Qt::red : palette.text()));
	fpu_pc_->setText(0, QString("PC: %1").arg(precisionMode));
	fpu_pc_->setForeground(0, QBrush((controlWord&(3<<10)) != (last_state_.fpu_control_word()&(3<<10)) ? Qt::red : palette.text()));
	fpu_rc_->setText(0, QString("RC: %1").arg(roundingMode));
	fpu_rc_->setForeground(0, QBrush((controlWord&(3<<8)) != (last_state_.fpu_control_word()&(3<<8)) ? Qt::red : palette.text()));

	register_view_items_[itemNumber]->setText(0, QString("FSR: %1").arg(statusWord.toHexString()));
	register_view_items_[itemNumber++]->setForeground(0, QBrush(statusWord != last_state_.fpu_status_word() ? Qt::red : palette.text()));
	fpu_exceptions_active_->setText(0, QString("Exceptions active:%1").arg(exceptionsHappenedString));
	fpu_exceptions_active_->setForeground(0, QBrush(exceptionsHappened != prevExHap ? Qt::red : palette.text()));
	fpu_status_->setText(0, QString("Status:%1").arg(fpuStatus));
	fpu_status_->setForeground(0, QBrush(fpuStatus!=getFPUStatus(last_state_.fpu_status_word()) ? Qt::red : palette.text()));
	fpu_top_->setText(0, QString("TOP: %1").arg(fpuTop));
	fpu_top_->setForeground(0, QBrush(fpuTop != last_state_.fpu_stack_pointer() ? Qt::red : palette.text()));

	register_view_items_[itemNumber]->setText(0, QString("FTAGS: %1").arg(state.fpu_tag_word().toHexString()));
	register_view_items_[itemNumber++]->setForeground(0, QBrush(state.fpu_tag_word() != last_state_.fpu_tag_word() ? Qt::red : palette.text()));

	{
		const Register FIP=state["FIP"];
		const Register FIS=state["FIS"];
		register_view_items_[itemNumber]->setText(0, QString("FIP: %1:%2").arg(FIS.toHexString()).arg(FIP.toHexString()));
		const Register oldFIP=last_state_["FIP"];
		const Register oldFIS=last_state_["FIS"];
		bool changed=FIP.toHexString()!=oldFIP.toHexString() || FIS.toHexString()!=oldFIS.toHexString();
		register_view_items_[itemNumber++]->setForeground(0, QBrush(changed ? Qt::red : palette.text()));
	}
	{
		const Register FDP=state["FDP"];
		const Register FDS=state["FDS"];
		register_view_items_[itemNumber]->setText(0, QString("FDP: %1:%2").arg(FDS.toHexString()).arg(FDP.toHexString()));
		const Register oldFDP=last_state_["FDP"];
		const Register oldFDS=last_state_["FDS"];
		bool changed=FDP.toHexString()!=oldFDP.toHexString() || FDS.toHexString()!=oldFDS.toHexString();
		register_view_items_[itemNumber++]->setForeground(0, QBrush(changed ? Qt::red : palette.text()));
	}
	{
		const Register fopc=state["fopcode"];
		// Yes, it appears big-endian!
		QString codeStr(!fopc ? "<unknown>" : edb::value8(fopc.value<edb::value16>() >> 8).toHexString()+" "+fopc.value<edb::value8>().toHexString());
		bool changed=fopc.toHexString()!=last_state_["fopcode"].toHexString();
		register_view_items_[itemNumber]->setText(0, QString("FOPC: %1").arg(codeStr));
		register_view_items_[itemNumber++]->setForeground(0, QBrush(changed ? Qt::red : palette.text()));
	}
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

	if(state.empty()) {
		for(auto item : register_view_items_)
			if(auto parent=item->parent())
				parent->setHidden(true);
		return;
	} else {
		for(auto item : register_view_items_)
			if(auto parent=item->parent())
				parent->setHidden(false);
		// and continue filling in the values
	}

	int itemNumber=0;
	for(std::size_t i=0;i<MAX_GPR_COUNT;++i) {
		update_register(register_view_items_[itemNumber], state.gp_register(i));
		register_view_items_[itemNumber++]->setForeground(0, (state.gp_register(i) != last_state_.gp_register(i)) ? Qt::red : palette.text());
	}

	const QString symname = edb::v1::find_function_symbol(state.instruction_pointer(), default_region_name);
	Register rIP=state.instruction_pointer_register();
	if(!symname.isEmpty()) {
		register_view_items_[itemNumber]->setText(0, QString("%0: %1 <%2>").arg(rIP.name().toUpper()).arg(rIP.toHexString()).arg(symname));
	} else {
		register_view_items_[itemNumber]->setText(0, QString("%0: %1").arg(rIP.name().toUpper()).arg(rIP.toHexString()));
	}
	register_view_items_[itemNumber++]->setForeground(0, (rIP != last_state_.instruction_pointer_register()) ? Qt::red : palette.text());

	Register flags=state.flags_register();
	Register flagsPrev=last_state_.flags_register();
	const bool flags_changed = flags != flagsPrev;
	if(flags_changed) {
		split_flags_->setText(0, state.flags_to_string());
	}

	register_view_items_[itemNumber]->setText(0, QString("%0: %1").arg(flags.name().toUpper()).arg(flags.toHexString()));
	register_view_items_[itemNumber++]->setForeground(0, flags_changed ? Qt::red : palette.text());

	const QString sregs[]={"es","cs","ss","ds","fs","gs"};
	for(std::size_t i=0;i<sizeof(sregs)/sizeof(sregs[0]);++i) {
		QString sreg(sregs[i]);
		auto sregValue=state[sreg].value<edb::seg_reg_t>();
		QString sregStr=sreg.toUpper()+QString(": %1").arg(sregValue.toHexString());
		const Register base=state[sregs[i]+"_base"];
		if(edb::v1::debuggeeIs32Bit() || i>=FS) {
			if(base)
				sregStr+=QString(" (%1)").arg(base.valueAsAddress().toHexString());
			else if(edb::v1::debuggeeIs32Bit() && sregValue==0)
				sregStr+=" NULL";
			else
				sregStr+=" (?)";
		}
		register_view_items_[itemNumber]->setText(0, sregStr);
		register_view_items_[itemNumber++]->setForeground(0, QBrush((state[sreg] != last_state_[sreg]) ? Qt::red : palette.text()));
	}

	update_fpu_view(itemNumber,state,palette);

	for(int i = 0; i < 8; ++i) {
		register_view_items_[itemNumber]->setText(0, QString("DR%1: %2").arg(i).arg(state.debug_register(i).toHexString()));
		register_view_items_[itemNumber++]->setForeground(0, QBrush((state.debug_register(i) != last_state_.debug_register(i)) ? Qt::red : palette.text()));
	}

	if(has_mmx_) {
		for(int i = 0; i < 8; ++i) {
			const Register current = state.mmx_register(i);
			const Register prev    = last_state_.mmx_register(i);
			QString valueStr;
			if(current) valueStr=formatSIMDRegister(current.value<MMWord>(),mmxDisplayMode_,mmxIntMode_);
			else valueStr=current.toHexString();
			register_view_items_[itemNumber]->setText(0, QString("MM%1: %2").arg(i).arg(valueStr));
			register_view_items_[itemNumber++]->setForeground(0, QBrush((current != prev) ? Qt::red : palette.text()));
		}
	}

	int padding=debuggeeIs64Bit() ? -2 : -1;
	if(has_ymm_) {
		for(std::size_t i = 0; i < MAX_YMM_REGS_COUNT; ++i) {
			const Register current = state.ymm_register(i);
			const Register prev    = last_state_.ymm_register(i);
			register_view_items_[itemNumber]->setHidden(!current);
			QString valueStr;
			if(current) valueStr=formatSIMDRegister(current.value<YMMWord>(),xymmDisplayMode_,xymmIntMode_);
			else valueStr=current.toHexString();
			register_view_items_[itemNumber]->setText(0, QString("YMM%1: %2").arg(i, padding).arg(valueStr));
			register_view_items_[itemNumber++]->setForeground(0, QBrush((current != prev) ? Qt::red : palette.text()));
		}
	} else if(has_xmm_) {
		for(std::size_t i = 0; i < MAX_XMM_REGS_COUNT; ++i) {
			const Register current = state.xmm_register(i);
			const Register prev    = last_state_.xmm_register(i);
			register_view_items_[itemNumber]->setHidden(!current);
			QString valueStr;
			if(current) valueStr=formatSIMDRegister(current.value<XMMWord>(),xymmDisplayMode_,xymmIntMode_);
			else valueStr=current.toHexString();
			register_view_items_[itemNumber]->setText(0, QString("XMM%1: %2").arg(i, padding).arg(valueStr));
			register_view_items_[itemNumber++]->setForeground(0, QBrush((current != prev) ? Qt::red : palette.text()));
		}
	}
	if(has_xmm_) {
	    const Register current = state["mxcsr"];
		if(current) {
			const Register prev    = last_state_["mxcsr"];
			const auto value = current.value<edb::value32>();

			mxcsr_exceptions_mask_->setText(0,"Exception mask   :"+getExceptionMaskString(value>>7));
			mxcsr_exceptions_mask_->setForeground(0, QBrush((prev.value<edb::value32>()&(0x3f<<7))!=(value&(0x3f<<7))? Qt::red : palette.text()));

			mxcsr_exceptions_active_->setText(0,"Exceptions active:"+getActiveExceptionsString(value));
			mxcsr_exceptions_active_->setForeground(0, QBrush((prev.value<edb::value32>()&0x3f)!=(value&0x3f)? Qt::red : palette.text()));

			mxcsr_daz_->setText(0,QString("DAZ: ")+((value&0x0040)?"on":"off"));
			mxcsr_daz_->setForeground(0, QBrush((prev.value<edb::value32>()&0x0040)!=(value&0x0040)? Qt::red : palette.text()));
			mxcsr_ftz_->setText(0,QString("FTZ: ")+((value&0x8000)?"on":"off"));
			mxcsr_ftz_->setForeground(0, QBrush((prev.value<edb::value32>()&0x8000)!=(value&0x8000)? Qt::red : palette.text()));

			static constexpr const int MXCSR_RC_LOW_BIT_POS=13;
			static constexpr const uint32_t MXCSR_RC_BITS=3<<MXCSR_RC_LOW_BIT_POS;
			mxcsr_rc_->setText(0,"RC: "+getRoundingMode((value&MXCSR_RC_BITS)>>MXCSR_RC_LOW_BIT_POS));
			mxcsr_rc_->setForeground(0, QBrush((value&MXCSR_RC_BITS) != (prev.value<edb::value32>()&MXCSR_RC_BITS) ? Qt::red : palette.text()));

			register_view_items_[itemNumber]->setText(0, QString("MXCSR: %1").arg(current.toHexString()));
			register_view_items_[itemNumber]->setForeground(0, QBrush((current != prev) ? Qt::red : palette.text()));
		} else {
			mxcsr_exceptions_mask_->setText(0,"???");
			mxcsr_exceptions_active_->setText(0,"???");
			mxcsr_ftz_->setText(0,"???");
			mxcsr_daz_->setText(0,"???");
			mxcsr_rc_->setText(0,"???");
		}
		++itemNumber;
	}

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

void ArchProcessor::setupMMXRegisterMenu(QMenu& menu) {
	{
		const auto displayModeMapper = new QSignalMapper(&menu);
		if(mmxDisplayMode_!=SIMDDisplayMode::Bytes) {
			const auto action = menu.addAction(tr("View MMX as &bytes"),displayModeMapper,SLOT(map()));
			displayModeMapper->setMapping(action,static_cast<int>(SIMDDisplayMode::Bytes));
		}
		if(mmxDisplayMode_!=SIMDDisplayMode::Words) {
			const auto action = menu.addAction(tr("View MMX as &words"),displayModeMapper,SLOT(map()));
			displayModeMapper->setMapping(action,static_cast<int>(SIMDDisplayMode::Words));
		}
		if(mmxDisplayMode_!=SIMDDisplayMode::Dwords) {
			const auto action = menu.addAction(tr("View MMX as &doublewords"),displayModeMapper,SLOT(map()));
			displayModeMapper->setMapping(action,static_cast<int>(SIMDDisplayMode::Dwords));
		}
		if(mmxDisplayMode_!=SIMDDisplayMode::Qwords) {
			const auto action = menu.addAction(tr("View MMX as &quadwords"),displayModeMapper,SLOT(map()));
			displayModeMapper->setMapping(action,static_cast<int>(SIMDDisplayMode::Qwords));
		}
		connect(displayModeMapper,SIGNAL(mapped(int)),this,SLOT(setMMXDisplayMode(int)));
	}

	menu.addSeparator();

	{
		const auto intModeMapper = new QSignalMapper(&menu);
		if(mmxIntMode_!=IntDisplayMode::Hex) {
			const auto action = menu.addAction(tr("View integers as &hexadecimal"),intModeMapper,SLOT(map()));
			intModeMapper->setMapping(action,static_cast<int>(IntDisplayMode::Hex));
		}
		if(mmxIntMode_!=IntDisplayMode::Signed) {
			const auto action = menu.addAction(tr("View integers as &signed decimal"),intModeMapper,SLOT(map()));
			intModeMapper->setMapping(action,static_cast<int>(IntDisplayMode::Signed));
		}
		if(mmxIntMode_!=IntDisplayMode::Unsigned) {
			const auto action = menu.addAction(tr("View integers as &unsigned decimal"),intModeMapper,SLOT(map()));
			intModeMapper->setMapping(action,static_cast<int>(IntDisplayMode::Unsigned));
		}
		connect(intModeMapper,SIGNAL(mapped(int)),this,SLOT(setMMXIntMode(int)));
	}
}

void ArchProcessor::setMMXIntMode(int mode) {
	mmxIntMode_=static_cast<IntDisplayMode>(mode);
}

void ArchProcessor::setMMXDisplayMode(int mode) {
	mmxDisplayMode_=static_cast<SIMDDisplayMode>(mode);
}

void ArchProcessor::setupSSEAVXRegisterMenu(QMenu& menu, const QString& extType) {
	{
		const auto displayModeMapper = new QSignalMapper(&menu);
		if(xymmDisplayMode_!=SIMDDisplayMode::Bytes) {
			const auto action = menu.addAction(tr("View %1 as &bytes").arg(extType),displayModeMapper,SLOT(map()));
			displayModeMapper->setMapping(action,static_cast<int>(SIMDDisplayMode::Bytes));
		}
		if(xymmDisplayMode_!=SIMDDisplayMode::Words) {
			const auto action = menu.addAction(tr("View %1 as &words").arg(extType),displayModeMapper,SLOT(map()));
			displayModeMapper->setMapping(action,static_cast<int>(SIMDDisplayMode::Words));
		}
		if(xymmDisplayMode_!=SIMDDisplayMode::Dwords) {
			const auto action = menu.addAction(tr("View %1 as &doublewords").arg(extType),displayModeMapper,SLOT(map()));
			displayModeMapper->setMapping(action,static_cast<int>(SIMDDisplayMode::Dwords));
		}
		if(xymmDisplayMode_!=SIMDDisplayMode::Qwords) {
			const auto action = menu.addAction(tr("View %1 as &quadwords").arg(extType),displayModeMapper,SLOT(map()));
			displayModeMapper->setMapping(action,static_cast<int>(SIMDDisplayMode::Qwords));
		}

		menu.addSeparator();

		if(xymmDisplayMode_!=SIMDDisplayMode::Floats32) {
			const auto action = menu.addAction(tr("View %1 as &32-bit floats").arg(extType),displayModeMapper,SLOT(map()));
			displayModeMapper->setMapping(action,static_cast<int>(SIMDDisplayMode::Floats32));
		}
		if(xymmDisplayMode_!=SIMDDisplayMode::Floats64) {
			const auto action = menu.addAction(tr("View %1 as &64-bit floats").arg(extType),displayModeMapper,SLOT(map()));
			displayModeMapper->setMapping(action,static_cast<int>(SIMDDisplayMode::Floats64));
		}
		connect(displayModeMapper,SIGNAL(mapped(int)),this,SLOT(setSSEAVXDisplayMode(int)));
	}

	if(xymmDisplayMode_!=SIMDDisplayMode::Floats32 && xymmDisplayMode_!=SIMDDisplayMode::Floats64) {

		menu.addSeparator();

		const auto intModeMapper = new QSignalMapper(&menu);
		if(xymmIntMode_!=IntDisplayMode::Hex) {
			const auto action = menu.addAction(tr("View integers as &hexadecimal"),intModeMapper,SLOT(map()));
			intModeMapper->setMapping(action,static_cast<int>(IntDisplayMode::Hex));
		}
		if(xymmIntMode_!=IntDisplayMode::Signed) {
			const auto action = menu.addAction(tr("View integers as &signed decimal"),intModeMapper,SLOT(map()));
			intModeMapper->setMapping(action,static_cast<int>(IntDisplayMode::Signed));
		}
		if(xymmIntMode_!=IntDisplayMode::Unsigned) {
			const auto action = menu.addAction(tr("View integers as &unsigned decimal"),intModeMapper,SLOT(map()));
			intModeMapper->setMapping(action,static_cast<int>(IntDisplayMode::Unsigned));
		}
		connect(intModeMapper,SIGNAL(mapped(int)),this,SLOT(setSSEAVXIntMode(int)));
	}
}

void ArchProcessor::setSSEAVXIntMode(int mode) {
	xymmIntMode_=static_cast<IntDisplayMode>(mode);
}

void ArchProcessor::setSSEAVXDisplayMode(int mode) {
	xymmDisplayMode_=static_cast<SIMDDisplayMode>(mode);
}

void ArchProcessor::setFPUDisplayMode(int mode) {
	fpuDisplayMode_=static_cast<FPUDisplayMode>(mode);
}

void ArchProcessor::setFPUOrderMode(int mode) {
	fpuOrderMode_=static_cast<FPUOrderMode>(mode);
}

void ArchProcessor::setupFPURegisterMenu(QMenu& menu) {
	{
		const auto displayModeMapper = new QSignalMapper(&menu);
		if(fpuDisplayMode_!=FPUDisplayMode::Hex) {
			const auto action = menu.addAction(tr("View FPU as raw &hex values"),displayModeMapper,SLOT(map()));
			displayModeMapper->setMapping(action,static_cast<int>(FPUDisplayMode::Hex));
		}
		if(fpuDisplayMode_!=FPUDisplayMode::Float) {
			const auto action = menu.addAction(tr("View FPU as 80-bit &floats"),displayModeMapper,SLOT(map()));
			displayModeMapper->setMapping(action,static_cast<int>(FPUDisplayMode::Float));
		}
		connect(displayModeMapper,SIGNAL(mapped(int)),this,SLOT(setFPUDisplayMode(int)));
	}

	menu.addSeparator();

	{
		const auto orderModeMapper = new QSignalMapper(&menu);
		if(fpuOrderMode_!=FPUOrderMode::Stack) {
			const auto action = menu.addAction(tr("View FPU as &stack of ST(i) registers"),orderModeMapper,SLOT(map()));
			orderModeMapper->setMapping(action,static_cast<int>(FPUOrderMode::Stack));
		}
		if(fpuOrderMode_!=FPUOrderMode::Independent) {
			const auto action = menu.addAction(tr("View FPU as &independent Ri registers"),orderModeMapper,SLOT(map()));
			orderModeMapper->setMapping(action,static_cast<int>(FPUOrderMode::Independent));
		}
		connect(orderModeMapper,SIGNAL(mapped(int)),this,SLOT(setFPUOrderMode(int)));
	}
}

std::unique_ptr<QMenu> ArchProcessor::register_item_context_menu(const Register& reg) {
	std::unique_ptr<QMenu> menu(new QMenu());

	if(reg.type()==Register::TYPE_SIMD && reg.name().startsWith("mm")) {
		setupMMXRegisterMenu(*menu);
		return menu;
	}
	if(reg.type()==Register::TYPE_SIMD && reg.name().startsWith("xmm")) {
		setupSSEAVXRegisterMenu(*menu,"SSE");
		return menu;
	}
	if(reg.type()==Register::TYPE_SIMD && reg.name().startsWith("ymm")) {
		setupSSEAVXRegisterMenu(*menu,"AVX");
		return menu;
	}
	if(reg.type()==Register::TYPE_FPU) {
		setupFPURegisterMenu(*menu);
		return menu;
	}

	return nullptr;
}
