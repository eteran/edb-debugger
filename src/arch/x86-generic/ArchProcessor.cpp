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

#include "ArchProcessor.h"
#include "Configuration.h"
#include "IDebugger.h"
#include "Instruction.h"
#include "Formatter.h"
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

#include <boost/math/special_functions/fpclassify.hpp>
#include <climits>
#include <cmath>

#ifdef Q_OS_LINUX
#include <asm/unistd.h>
#endif

namespace {

enum RegisterIndex {
	Rax  = 0, Eax  = Rax,
	Rcx  = 1, Ecx  = Rcx,
	Rdx  = 2, Edx  = Rdx,
	Rbx  = 3, Ebx  = Rbx,
	Rsp  = 4, Esp  = Rsp,
	Rbp  = 5, Ebp  = Rbp,
	Rsi  = 6, Esi  = Rsi,
	Rdi  = 7, Edi  = Rdi,
	R8   = 8, Eip  = R8, // XXX: this is ugly
	R9   = 9,
	R10  = 10,
	R11  = 11,
	R12  = 12,
	R13  = 13,
	R14  = 14,
	R15  = 15,
	RIP  = 16
};

constexpr int FUNC_PARAM_REGS_COUNT=IS_X86_32_BIT ? 0 : 6;
static constexpr std::size_t GPR_COUNT=IS_X86_32_BIT ? 8 : 16;
static constexpr std::size_t XMM_REG_COUNT=GPR_COUNT;
static constexpr std::size_t YMM_REG_COUNT=GPR_COUNT;
const std::array<const char*,GPR_COUNT> GPRegNames={
#ifdef EDB_X86
	"EAX",
	"ECX",
	"EDX",
	"EBX",
	"ESP",
	"EBP",
	"ESI",
	"EDI"
#elif defined EDB_X86_64
	"RAX",
	"RCX",
	"RDX",
	"RBX",
	"RSP",
	"RBP",
	"RSI",
	"RDI",
	"R8",
	"R9",
	"R10",
	"R11",
	"R12",
	"R13",
	"R14",
	"R15"
#endif
};
constexpr const char* const IP_name=IS_X86_32_BIT ? "EIP" : "RIP";
constexpr const char* const FLAGS_name=IS_X86_32_BIT ? "EFLAGS" : "RFLAGS";

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
edb::address_t get_effective_address(const edb::Operand &op, const State &state) {
	edb::address_t ret = 0;

	// TODO: get registers by index, not string! too slow
	
	if(op.valid()) {
		switch(op.general_type()) {
		case edb::Operand::TYPE_REGISTER:
			ret = state[QString::fromStdString(edb::v1::formatter().to_string(op))].value<edb::reg_t>();
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

				if(op.owner()->prefix() & edb::Instruction::PREFIX_GS) {
					ret += state["gs_base"].valueAsAddress();
				}

				if(op.owner()->prefix() & edb::Instruction::PREFIX_FS) {
					ret += state["fs_base"].valueAsAddress();
				}
			} while(0);
			break;
		case edb::Operand::TYPE_ABSOLUTE:
			ret = op.absolute().offset;
			if(op.owner()->prefix() & edb::Instruction::PREFIX_GS) {
				ret += state["gs_base"].valueAsAddress();
			}

			if(op.owner()->prefix() & edb::Instruction::PREFIX_FS) {
				ret += state["fs_base"].valueAsAddress();
			}
			break;
		case edb::Operand::TYPE_IMMEDIATE:
			break;
		case edb::Operand::TYPE_REL:
			ret = op.relative_target();
			break;
		default:
			break;
		}
	}
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
		return QString("0x%1").arg(edb::v1::format_pointer(arg));
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
		if(arg < 0x80 && (std::isprint(arg) || std::isspace(arg))) {
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
QString format_char(int pointer_level, edb::reg_t arg, QChar type) {

	if(IProcess *process = edb::v1::debugger_core->process()) {
		if(pointer_level == 1) {
			if(arg == 0) {
				return "NULL";
			} else {
				QString string_param;
				int string_length;

				if(edb::v1::get_ascii_string_at_address(arg, string_param, edb::v1::config().min_string_length, 256, string_length)) {
					return QString("<0x%1> \"%2\"").arg(edb::v1::format_pointer(arg)).arg(string_param);
				} else {
					char character;
					process->read_bytes(arg, &character, sizeof(character));
					if(character == '\0') {
						return QString("<0x%1> \"\"").arg(edb::v1::format_pointer(arg));
					} else {
						return QString("<0x%1>").arg(edb::v1::format_pointer(arg));
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
QString format_argument(const QString &type, edb::reg_t arg) {
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
			case 'v': return format_pointer(pointer_level, arg, ch);
			case 'w': return format_integer(pointer_level, arg, ch);
			case 'b': return format_integer(pointer_level, arg, ch);
			case 'c': return format_char(pointer_level, arg, ch);
			case 'a': return format_integer(pointer_level, arg, ch);
			case 'h': return format_integer(pointer_level, arg, ch);
			case 's': return format_integer(pointer_level, arg, ch);
			case 't': return format_integer(pointer_level, arg, ch);
			case 'i': return format_integer(pointer_level, arg, ch);
			case 'j': return format_integer(pointer_level, arg, ch);
			case 'l': return format_integer(pointer_level, arg, ch);
			case 'm': return format_integer(pointer_level, arg, ch);
			case 'x': return format_integer(pointer_level, arg, ch);
			case 'y': return format_integer(pointer_level, arg, ch);
			case 'n': return format_integer(pointer_level, arg, ch);
			case 'o': return format_integer(pointer_level, arg, ch);
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

	return format_pointer(pointer_level, arg, 'x');
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
	static const char *const parameter_registers[FUNC_PARAM_REGS_COUNT] = {
	#ifdef EDB_X86
	#elif defined EDB_X86_64
		"rdi",
		"rsi",
		"rdx",
		"rcx",
		"r8",
		"r9"
	#endif
	};

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

				edb::reg_t arg;
				if(i+1 > FUNC_PARAM_REGS_COUNT) {
					size_t arg_i_position=(i - FUNC_PARAM_REGS_COUNT) * sizeof(edb::reg_t);
					process->read_bytes(state.stack_pointer() + offset + arg_i_position, &arg, sizeof(arg));
				} else {
					arg = state[parameter_registers[i]].value<edb::reg_t>();
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
	if(cond==edb::Instruction::CC_RCXZ) return state.gp_register(Rcx).value<edb::value64>() == 0;
	if(cond==edb::Instruction::CC_ECXZ) return state.gp_register(Rcx).value<edb::value32>() == 0;
	if(cond==edb::Instruction::CC_CXZ)  return state.gp_register(Rcx).value<edb::value16>() == 0;

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
		edb::address_t return_address;
		process->read_bytes(state.stack_pointer(), &return_address, sizeof(return_address));
	
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
		
			const edb::address_t effective_address = get_effective_address(operand, state);
			const QString temp_operand             = QString::fromStdString(edb::v1::formatter().to_string(operand));
			QString temp;

			switch(operand.general_type()) {
			case edb::Operand::TYPE_REL:
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
					edb::address_t target;

					if(process->read_bytes(effective_address, &target, sizeof(target))) {
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
						const edb::address_t effective_address = get_effective_address(operand, state);
						ret << QString("%1 = %2").arg(temp_operand).arg(edb::v1::format_pointer(effective_address));
					} while(0);
					break;
				case edb::Operand::TYPE_REGISTER:
					{
						Register reg=state[QString::fromStdString(edb::v1::formatter().to_string(operand))];
						QString valueString;
						if(!reg) {
							valueString = ArchProcessor::tr("(Error: obtained invalid register value from State)");
						} else {
							switch(reg.bitSize()) {
							case 8:
								valueString=reg.value<edb::value8>().toHexString();
								break;
							case 16:
								valueString=reg.value<edb::value16>().toHexString();
								break;
							case 32:
								valueString=reg.value<edb::value32>().toHexString();
								break;
							case 64:
								valueString=reg.value<edb::value64>().toHexString();
								break;
							case 80:
								valueString=reg.value<edb::value80>().toHexString();
								break;
							case 128:
								valueString=reg.value<edb::value128>().toHexString();
								break;
							default:
								valueString = ArchProcessor::tr("(Error: unexpected register size %1)").arg(reg.bitSize());
							}
						}
						ret << QString("%1 = %2").arg(temp_operand).arg(valueString);
						break;
					}
				case edb::Operand::TYPE_EXPRESSION:
					do {
						const edb::address_t effective_address = get_effective_address(operand, state);
						edb::value128 target;

						if(process->read_bytes(effective_address, &target, sizeof(target))) {
							switch(operand.complete_type()) {
							case edb::Operand::TYPE_EXPRESSION8:
								ret << QString("%1 = [%2] = 0x%3").arg(temp_operand).arg(edb::v1::format_pointer(effective_address)).arg(edb::value8(target).toHexString());
								break;
							case edb::Operand::TYPE_EXPRESSION16:
								ret << QString("%1 = [%2] = 0x%3").arg(temp_operand).arg(edb::v1::format_pointer(effective_address)).arg(edb::value16(target).toHexString());
								break;
							case edb::Operand::TYPE_EXPRESSION32:
								ret << QString("%1 = [%2] = 0x%3").arg(temp_operand).arg(edb::v1::format_pointer(effective_address)).arg(edb::value32(target).toHexString());
								break;
							case edb::Operand::TYPE_EXPRESSION64:
								ret << QString("%1 = [%2] = 0x%3").arg(temp_operand).arg(edb::v1::format_pointer(effective_address)).arg(edb::value64(target).toHexString());
								break;
							case edb::Operand::TYPE_EXPRESSION80:
								ret << QString("%1 = [%2] = 0x%3").arg(temp_operand).arg(edb::v1::format_pointer(effective_address)).arg(edb::value80(target).toHexString());
								break;
							case edb::Operand::TYPE_EXPRESSION128:
								ret << QString("%1 = [%2] = 0x%3").arg(temp_operand).arg(edb::v1::format_pointer(effective_address)).arg(edb::value128(target).toHexString());
								break;
							default:
								ret << QString("%1 = [%2] = 0x%3").arg(temp_operand).arg(edb::v1::format_pointer(effective_address)).arg(edb::reg_t(target).toHexString());
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
			edb::Instruction inst(buffer, buffer + sz, addr, std::nothrow);
			if(is_jump(inst)) {
				const edb::Operand &operand = inst.operands()[0];

				if(operand.general_type() == edb::Operand::TYPE_REL) {
					const edb::address_t target = operand.relative_target();

					if(target == address) {
						ret << ArchProcessor::tr("possible jump from 0x%1").arg(edb::v1::format_pointer(addr));
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
void analyze_syscall(const State &state, const edb::Instruction &inst, QStringList &ret) {
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
		static const QString arch=IS_X86_32_BIT ? "x86" : "x86-64";
		// Index Eax is equal to Rax
		query.setQuery(QString("syscalls[@version='1.0']/linux[@arch='"+arch+"']/syscall[index=%1]").arg(state.gp_register(Eax).value<edb::reg_t>()));
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
			arguments << format_argument(argument_type, state[argument_register].value<edb::reg_t>());
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

	if(edb::v1::debugger_core) {
		State state;

		Q_ASSERT(category_list);

		// setup the register view
		if(QTreeWidgetItem *const gpr = category_list->addCategory(tr("General Purpose"))) {
			for(std::size_t i=0;i<GPR_COUNT;++i)
				register_view_items_.push_back(create_register_item(gpr, GPRegNames[i]));
			register_view_items_.push_back(create_register_item(gpr, IP_name));
			register_view_items_.push_back(create_register_item(gpr, FLAGS_name));

			// split [ER]FLAGS view
			split_flags_ = new QTreeWidgetItem(register_view_items_.back());
			split_flags_->setText(0, state.flags_to_string(0));
		}

		if(QTreeWidgetItem *const segs = category_list->addCategory(tr("Segments"))) {
			register_view_items_.push_back(create_register_item(segs, "cs"));
			register_view_items_.push_back(create_register_item(segs, "ds"));
			register_view_items_.push_back(create_register_item(segs, "es"));
			register_view_items_.push_back(create_register_item(segs, "fs"));
			register_view_items_.push_back(create_register_item(segs, "gs"));
			register_view_items_.push_back(create_register_item(segs, "ss"));
		}

		if(QTreeWidgetItem *const fpu = category_list->addCategory(tr("FPU"))) {
			register_view_items_.push_back(create_register_item(fpu, "R0"));
			register_view_items_.push_back(create_register_item(fpu, "R1"));
			register_view_items_.push_back(create_register_item(fpu, "R2"));
			register_view_items_.push_back(create_register_item(fpu, "R3"));
			register_view_items_.push_back(create_register_item(fpu, "R4"));
			register_view_items_.push_back(create_register_item(fpu, "R5"));
			register_view_items_.push_back(create_register_item(fpu, "R6"));
			register_view_items_.push_back(create_register_item(fpu, "R7"));
			register_view_items_.push_back(create_register_item(fpu, "Control Word"));
			register_view_items_.push_back(create_register_item(fpu, "PC"));
			register_view_items_.push_back(create_register_item(fpu, "RC"));
			register_view_items_.push_back(create_register_item(fpu, "Status Word"));
			register_view_items_.push_back(create_register_item(fpu, "TOP"));
			register_view_items_.push_back(create_register_item(fpu, "Tag Word"));
		}

		if(QTreeWidgetItem *const dbg = category_list->addCategory(tr("Debug"))) {
			register_view_items_.push_back(create_register_item(dbg, "dr0"));
			register_view_items_.push_back(create_register_item(dbg, "dr1"));
			register_view_items_.push_back(create_register_item(dbg, "dr2"));
			register_view_items_.push_back(create_register_item(dbg, "dr3"));
			register_view_items_.push_back(create_register_item(dbg, "dr4"));
			register_view_items_.push_back(create_register_item(dbg, "dr5"));
			register_view_items_.push_back(create_register_item(dbg, "dr6"));
			register_view_items_.push_back(create_register_item(dbg, "dr7"));
		}

		if(has_mmx_) {
			if(QTreeWidgetItem *const mmx = category_list->addCategory(tr("MMX"))) {
				register_view_items_.push_back(create_register_item(mmx, "mm0"));
				register_view_items_.push_back(create_register_item(mmx, "mm1"));
				register_view_items_.push_back(create_register_item(mmx, "mm2"));
				register_view_items_.push_back(create_register_item(mmx, "mm3"));
				register_view_items_.push_back(create_register_item(mmx, "mm4"));
				register_view_items_.push_back(create_register_item(mmx, "mm5"));
				register_view_items_.push_back(create_register_item(mmx, "mm6"));
				register_view_items_.push_back(create_register_item(mmx, "mm7"));
			}
		}

		if(has_ymm_) {
			if(QTreeWidgetItem *const ymm = category_list->addCategory(tr("AVX"))) {
				for(std::size_t i=0;i<YMM_REG_COUNT;++i)
					register_view_items_.push_back(create_register_item(ymm, QString("YMM%1").arg(i)));
				register_view_items_.push_back(create_register_item(ymm, "mxcsr"));
			}
		} else if(has_xmm_) {
			if(QTreeWidgetItem *const xmm = category_list->addCategory(tr("SSE"))) {
				for(std::size_t i=0;i<XMM_REG_COUNT;++i)
					register_view_items_.push_back(create_register_item(xmm, QString("XMM%1").arg(i)));
				register_view_items_.push_back(create_register_item(xmm, "mxcsr"));
			}
		}

		update_register_view(QString(), State());
	}
}

//------------------------------------------------------------------------------
// Name: value_from_item
// Desc:
//------------------------------------------------------------------------------
Register ArchProcessor::value_from_item(const QTreeWidgetItem &item) {
	const QString &name = item.data(0, Qt::UserRole).toString();
	State state;
	edb::v1::debugger_core->get_state(&state);
	return state[name];
}

//------------------------------------------------------------------------------
// Name: update_register
// Desc:
//------------------------------------------------------------------------------
void ArchProcessor::update_register(QTreeWidgetItem *item, const QString &name_, const Register &reg) const {

	Q_ASSERT(item);

	QString reg_string;
	int string_length;
	const edb::reg_t value = reg.value<edb::reg_t>();
	const QString name=IS_X86_64_BIT ? name_.leftJustified(3) : name_;

	if(edb::v1::get_ascii_string_at_address(value, reg_string, edb::v1::config().min_string_length, 256, string_length)) {
		item->setText(0, QString("%1: %2 ASCII \"%3\"").arg(name, edb::v1::format_pointer(value), reg_string));
	} else if(edb::v1::get_utf16_string_at_address(value, reg_string, edb::v1::config().min_string_length, 256, string_length)) {
		item->setText(0, QString("%1: %2 UTF16 \"%3\"").arg(name, edb::v1::format_pointer(value), reg_string));
	} else {
		item->setText(0, QString("%1: %2").arg(name, edb::v1::format_pointer(value)));
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

//------------------------------------------------------------------------------
// Name: update_fpu_view
// Desc:
//------------------------------------------------------------------------------
void ArchProcessor::update_fpu_view(int& itemNumber, const State &state, const QPalette& palette) const {

	const int fpuTop=state.fpu_stack_pointer();
	for(int i = 0; i < 8; ++i) {
		const edb::value80 current = state.fpu_register(i);
		const edb::value80 prev    = last_state_.fpu_register(i);
		bool empty=state.fpu_register_is_empty(i);
		const QString tag=state.fpu_register_tag_string(i);

		QString typeString;
		QString valueString;
		if(!empty) {
			auto type=current.floatType();
			if(current.isSpecial(type))
				typeString=" "+current.floatTypeString(type);
			long double value=current.toFloatValue();
			if(!std::isnan(value) && !std::isinf(value))
				valueString=" "+current.toString();
		}

		register_view_items_[itemNumber]->setText(0, QString("%1R%2: %3 %4%5%6").arg(fpuTop==i?"=>":"  ").arg(i).arg(tag.leftJustified(8)).arg(current.toHexString()).arg(valueString).arg(typeString));
		register_view_items_[itemNumber++]->setForeground(0, QBrush((current != prev) ? Qt::red : palette.text()));
	}
	edb::value16 controlWord=state.fpu_control_word();
	int controlWordValue=controlWord;
	edb::value16 statusWord=state.fpu_status_word();
	QString roundingMode;
	QString precisionMode;
	switch((controlWordValue>>10)&3) {
	case 0:
		roundingMode = tr("Rounding to nearest");
		break;
	case 1:
		roundingMode = tr("Rounding down");
		break;
	case 2:
		roundingMode = tr("Rounding up");
		break;
	case 3:
		roundingMode = tr("Rounding toward zero");
		break;
	}
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
	QString exceptionMaskString;
	exceptionMaskString += ((exceptionMask & 0x01) ? " IM" : " Iu");
	exceptionMaskString += ((exceptionMask & 0x02) ? " DM" : " Du");
	exceptionMaskString += ((exceptionMask & 0x04) ? " ZM" : " Zu");
	exceptionMaskString += ((exceptionMask & 0x08) ? " OM" : " Ou");
	exceptionMaskString += ((exceptionMask & 0x10) ? " UM" : " Uu");
	exceptionMaskString += ((exceptionMask & 0x20) ? " PM" : " Pu");
	QString exceptionsHappenedString;
	exceptionsHappenedString += ((exceptionsHappened & 0x01) ? " IE" : "");
	exceptionsHappenedString += ((exceptionsHappened & 0x02) ? " DE" : "");
	exceptionsHappenedString += ((exceptionsHappened & 0x04) ? " ZE" : "");
	exceptionsHappenedString += ((exceptionsHappened & 0x08) ? " OE" : "");
	exceptionsHappenedString += ((exceptionsHappened & 0x10) ? " UE" : "");
	exceptionsHappenedString += ((exceptionsHappened & 0x20) ? " PE" : "");
	bool stackFault=statusWord&0x40;
	if(stackFault)
		exceptionsHappenedString += " SF";
	bool fpuBusy=statusWord&0x8000;
	QString fpuBusyString;
	if(fpuBusy)
		fpuBusyString=" BUSY";
	QString stackFaultDetail;
	bool invalidOperationException=(exceptionsHappened & 0x01);
	bool C1=statusWord&(1<<9);
	if(invalidOperationException && stackFault) {
		stackFaultDetail=C1?" (stack overflow)":" (stack underflow)";
	}
	register_view_items_[itemNumber++]->setText(0, QString("Control Word: %1   %2").arg(controlWord.toHexString()).arg(exceptionMaskString));
	register_view_items_[itemNumber++]->setText(0, QString("  PC: %1").arg(precisionMode));
	register_view_items_[itemNumber++]->setText(0, QString("  RC: %1").arg(roundingMode));
	register_view_items_[itemNumber++]->setText(0, QString("Status Word: %1   %2%3%4").arg(statusWord.toHexString()).arg(exceptionsHappenedString).arg(fpuBusyString).arg(stackFaultDetail));
	register_view_items_[itemNumber++]->setText(0, QString("  TOP: %1").arg(fpuTop));
	register_view_items_[itemNumber++]->setText(0, QString("Tag Word: %1").arg(state.fpu_tag_word().toHexString()));
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
	for(std::size_t i=0;i<GPR_COUNT;++i)
		update_register(register_view_items_[itemNumber++], GPRegNames[i], state.gp_register(i));

	const QString symname = edb::v1::find_function_symbol(state.instruction_pointer(), default_region_name);

	if(!symname.isEmpty()) {
		register_view_items_[itemNumber++]->setText(0, QString("%0: %1 <%2>").arg(IP_name).arg(edb::v1::format_pointer(state.instruction_pointer())).arg(symname));
	} else {
		register_view_items_[itemNumber++]->setText(0, QString("%0: %1").arg(IP_name).arg(edb::v1::format_pointer(state.instruction_pointer())));
	}
	register_view_items_[itemNumber++]->setText(0, QString("%0: %1").arg(FLAGS_name).arg(edb::v1::format_pointer(state.flags())));

	register_view_items_[itemNumber++]->setText(0, QString("CS: %1")     .arg(state["cs"].value<edb::seg_reg_t>().toHexString()));
	register_view_items_[itemNumber++]->setText(0, QString("DS: %1")     .arg(state["ds"].value<edb::seg_reg_t>().toHexString()));
	register_view_items_[itemNumber++]->setText(0, QString("ES: %1")     .arg(state["es"].value<edb::seg_reg_t>().toHexString()));
	register_view_items_[itemNumber++]->setText(0, QString("FS: %1 (%2)").arg(state["fs"].value<edb::seg_reg_t>().toHexString()).arg(edb::v1::format_pointer(state["fs_base"].value<edb::reg_t>())));
	register_view_items_[itemNumber++]->setText(0, QString("GS: %1 (%2)").arg(state["gs"].value<edb::seg_reg_t>().toHexString()).arg(edb::v1::format_pointer(state["gs_base"].value<edb::reg_t>())));
	register_view_items_[itemNumber++]->setText(0, QString("SS: %1")     .arg(state["ss"].value<edb::seg_reg_t>().toHexString()));

	update_fpu_view(itemNumber,state,palette);

	for(int i = 0; i < 8; ++i) {
		register_view_items_[itemNumber]->setText(0, QString("DR%1: %2").arg(i).arg(state.debug_register(i).toHexString()));
		register_view_items_[itemNumber++]->setForeground(0, QBrush((state.debug_register(i) != last_state_.debug_register(i)) ? Qt::red : palette.text()));
	}

	if(has_mmx_) {
		for(int i = 0; i < 8; ++i) {
			const edb::value64 current = state.mmx_register(i);
			const edb::value64 prev    = last_state_.mmx_register(i);
			register_view_items_[itemNumber]->setText(0, QString("MM%1: %2").arg(i).arg(current.toHexString()));
			register_view_items_[itemNumber++]->setForeground(0, QBrush((current != prev) ? Qt::red : palette.text()));
		}
	}

	int padding=IS_X86_64_BIT ? -2 : -1;
	if(has_ymm_) {
		for(std::size_t i = 0; i < YMM_REG_COUNT; ++i) {
			const edb::value256 current = state.ymm_register(i);
			const edb::value256 prev    = last_state_.ymm_register(i);
			register_view_items_[itemNumber]->setText(0, QString("YMM%1: %2").arg(i, padding).arg(current.toHexString()));
			register_view_items_[itemNumber++]->setForeground(0, QBrush((current != prev) ? Qt::red : palette.text()));
		}
	} else if(has_xmm_) {
		for(std::size_t i = 0; i < XMM_REG_COUNT; ++i) {
			const edb::value128 current = state.xmm_register(i);
			const edb::value128 prev    = last_state_.xmm_register(i);
			register_view_items_[itemNumber]->setText(0, QString("XMM%1: %2").arg(i, padding).arg(current.toHexString()));
			register_view_items_[itemNumber++]->setForeground(0, QBrush((current != prev) ? Qt::red : palette.text()));
		}
	}
	if(has_xmm_ || has_ymm_) {
		const edb::value32 current = state["mxcsr"].value<edb::value32>();
		const edb::value32 prev    = last_state_["mxcsr"].value<edb::value32>();
		register_view_items_[itemNumber]->setText(0, QString("MXCSR: %1").arg(current.toHexString()));
		register_view_items_[itemNumber++]->setForeground(0, QBrush((current != prev) ? Qt::red : palette.text()));
	}

	const bool flags_changed = state.flags() != last_state_.flags();
	if(flags_changed) {
		split_flags_->setText(0, state.flags_to_string());
	}

	// highlight any changed registers
	itemNumber=0;
	for(std::size_t i=0;i<GPR_COUNT;++i)
		register_view_items_[itemNumber++]->setForeground(0, (state.gp_register(i) != last_state_.gp_register(i)) ? Qt::red : palette.text());
	register_view_items_[itemNumber++]->setForeground(0, (state.instruction_pointer() != last_state_.instruction_pointer()) ? Qt::red : palette.text());
	register_view_items_[itemNumber++]->setForeground(0, flags_changed ? Qt::red : palette.text());

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
			edb::Instruction inst(buffer, buffer + sizeof(buffer), address, std::nothrow);
			if(inst) {

				State state;
				edb::v1::debugger_core->get_state(&state);

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
				   if(inst.operands()[0].complete_type() == edb::Operand::TYPE_IMMEDIATE8 && (inst.operands()[0].immediate() & 0xff) == 0x80) {

						analyze_syscall(state, inst, ret);
					} else {

						analyze_operands(state, inst, ret);
					}
				#endif
				} else if (inst.is_syscall()) {

					analyze_syscall(state, inst, ret);

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
