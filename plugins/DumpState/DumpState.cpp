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

#include "DumpState.h"
#include "IDebugger.h"
#include "Instruction.h"
#include "OptionsPage.h"
#include "State.h"
#include "Util.h"
#include "edb.h"

#include <QMenu>
#include <QSettings>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace DumpState {
namespace {

//------------------------------------------------------------------------------
// Name: hex_string
// Desc:
//------------------------------------------------------------------------------
template <class T>
std::string hex_string(const T& value) {
	return value.toHexString().toStdString();
}

}

//------------------------------------------------------------------------------
// Name: DumpState
// Desc:
//------------------------------------------------------------------------------
DumpState::DumpState() : menu_(0) {
}

//------------------------------------------------------------------------------
// Name: ~DumpState
// Desc:
//------------------------------------------------------------------------------
DumpState::~DumpState() {
}

//------------------------------------------------------------------------------
// Name: menu
// Desc:
//------------------------------------------------------------------------------
QMenu *DumpState::menu(QWidget *parent) {

	Q_ASSERT(parent);

	if(!menu_) {
		menu_ = new QMenu(tr("DumpState"), parent);
		menu_->addAction (tr("&Dump Current State"), this, SLOT(show_menu()), QKeySequence(tr("Ctrl+D")));
	}

	return menu_;
}

//------------------------------------------------------------------------------
// Name: dump_code
// Desc:
//------------------------------------------------------------------------------
void DumpState::dump_code(const State &state) {

	QSettings settings;
	const int instructions_to_print = settings.value("DumpState/instructions_after_ip", 6).toInt();

	const edb::address_t ip = state.instruction_pointer();
	edb::address_t address = ip;

	for(int i = 0; i < instructions_to_print + 1; ++i) {
		quint8 buf[edb::Instruction::MAX_SIZE];
		if(const int size = edb::v1::get_instruction_bytes(address, buf)) {
			edb::Instruction inst(buf, buf + size, address);
			if(inst) {
				std::cout << ((address == ip) ? "> " : "  ") << hex_string(address) << ": " << edb::v1::formatter().to_string(inst) << "\n";
			} else {
				break;
			}
			address += inst.size();
		} else {
			break;
		}
	}
}

//------------------------------------------------------------------------------
// Name: dump_registers
// Desc:
//------------------------------------------------------------------------------
void DumpState::dump_registers(const State &state) {

	using std::cout;
	if(edb::v1::debuggeeIs32Bit()) { // TODO: check if state itself is 32 bit, not current debuggee. Generally it's not the same.
		cout << "     eax:" <<    hex_string(state["eax"]);
		cout << " ecx:" <<        hex_string(state["ecx"]);
		cout << "  edx:" <<       hex_string(state["edx"]);
		cout << "  ebx:" <<       hex_string(state["ebx"]);
		cout << "     eflags:" << hex_string(state["eflags"]);
		cout << "\n";
		cout << "     esp:" << hex_string(state["esp"]);
		cout << " ebp:" <<     hex_string(state["ebp"]);
		cout << "  esi:" <<    hex_string(state["esi"]);
		cout << "  edi:" <<    hex_string(state["edi"]);
		cout << "     eip:" << hex_string(state["eip"]);
		cout << "\n";
		cout << "     es:" << hex_string(state["es"]);
		cout << "  cs:" <<    hex_string(state["cs"]);
		cout << "  ss:" <<    hex_string(state["ss"]);
		cout << "  ds:" <<    hex_string(state["ds"]);
		cout << "  fs:" <<    hex_string(state["fs"]);
		cout << "  gs:" <<    hex_string(state["gs"]);
		cout << "    ";
		const Register eflagsR=state["eflags"];
		if(eflagsR) {
			const auto eflags=eflagsR.value<edb::value32>();
			cout << ((eflags & (1 << 11)) != 0 ? 'O' : 'o') << ' ';
			cout << ((eflags & (1 << 10)) != 0 ? 'D' : 'd') << ' ';
			cout << ((eflags & (1 <<  9)) != 0 ? 'I' : 'i') << ' ';
			cout << ((eflags & (1 <<  8)) != 0 ? 'T' : 't') << ' ';
			cout << ((eflags & (1 <<  7)) != 0 ? 'S' : 's') << ' ';
			cout << ((eflags & (1 <<  6)) != 0 ? 'Z' : 'z') << ' ';
			cout << ((eflags & (1 <<  4)) != 0 ? 'A' : 'a') << ' ';
			cout << ((eflags & (1 <<  2)) != 0 ? 'P' : 'p') << ' ';
			cout << ((eflags & (1 <<  0)) != 0 ? 'C' : 'c');
		}
		cout << "\n";
	} else {
		cout << "     rax:" <<    hex_string(state["rax"]);
		cout << " rcx:" <<        hex_string(state["rcx"]);
		cout << "  rdx:" <<       hex_string(state["rdx"]);
		cout << "  rbx:" <<       hex_string(state["rbx"]);
		cout << "     rflags:" << hex_string(state["rflags"]);
		cout << "\n";
		cout << "     rsp:" <<    hex_string(state["rsp"]);
		cout << " rbp:" <<        hex_string(state["rbp"]);
		cout << "  rsi:" <<       hex_string(state["rsi"]);
		cout << "  rdi:" <<       hex_string(state["rdi"]);
		cout << "        rip:" << hex_string(state["rip"]);
		cout << "\n";
		cout << "      r8:" << hex_string(state["r8"]);
		cout << "  r9:" <<     hex_string(state["r9"]);
		cout << "  r10:" <<    hex_string(state["r10"]);
		cout << "  r11:" <<    hex_string(state["r11"]);
		cout << "           ";
		const Register rflagsR=state["rflags"];
		if(rflagsR) {
			const auto rflags=rflagsR.value<edb::value32>();
			cout << ((rflags & (1 << 11)) != 0 ? 'O' : 'o') << ' ';
			cout << ((rflags & (1 << 10)) != 0 ? 'D' : 'd') << ' ';
			cout << ((rflags & (1 <<  9)) != 0 ? 'I' : 'i') << ' ';
			cout << ((rflags & (1 <<  8)) != 0 ? 'T' : 't') << ' ';
			cout << ((rflags & (1 <<  7)) != 0 ? 'S' : 's') << ' ';
			cout << ((rflags & (1 <<  6)) != 0 ? 'Z' : 'z') << ' ';
			cout << ((rflags & (1 <<  4)) != 0 ? 'A' : 'a') << ' ';
			cout << ((rflags & (1 <<  2)) != 0 ? 'P' : 'p') << ' ';
			cout << ((rflags & (1 <<  0)) != 0 ? 'C' : 'c');
		}
		cout << "\n";
		cout << "     r12:" << hex_string(state["r12"]);
		cout << " r13:" <<     hex_string(state["r13"]);
		cout << "  r14:" <<    hex_string(state["r14"]);
		cout << "  r15:" <<    hex_string(state["r15"]);
		cout << "\n";
		cout << "     es:" << hex_string(state["es"]);
		cout << "  cs:" <<    hex_string(state["cs"]);
		cout << "  ss:" <<    hex_string(state["ss"]);
		cout << "  ds:" <<    hex_string(state["ds"]);
		cout << "  fs:" <<    hex_string(state["fs"]);
		cout << "  gs:" <<    hex_string(state["gs"]);
		cout << "\n";
	}
}

//------------------------------------------------------------------------------
// Name: dump_lines
// Desc:
//------------------------------------------------------------------------------
void DumpState::dump_lines(edb::address_t address, int lines) {
	
	if(IProcess *process = edb::v1::debugger_core->process()) {
		for(int i = 0; i < lines; ++i) {
			edb::value8 buf[16];
			if(process->read_bytes(address, buf, sizeof(buf))) {
				std::cout << hex_string(address) << " : ";

				for(int j = 0x00; j < 0x04; ++j) std::cout << hex_string(buf[j]) << " ";
				std::cout << " ";
				for(int j = 0x04; j < 0x08; ++j) std::cout << hex_string(buf[j]) << " ";
				std::cout << "- ";
				for(int j = 0x08; j < 0x0c; ++j) std::cout << hex_string(buf[j]) << " ";
				std::cout << " ";
				for(int j = 0x0c; j < 0x10; ++j) std::cout << hex_string(buf[j]) << " ";

				for(int j = 0; j < 16; ++j) {
					const quint8 ch = buf[j];
					std::cout << ((std::isprint(ch) || (std::isspace(ch) && (ch != '\f' && ch != '\t' && ch != '\r' && ch != '\n') && ch < 0x80)) ? static_cast<char>(ch) : '.');
				}

				std::cout << "\n";
			} else {
				break;
			}
			address += 16;
		}
	}
}

//------------------------------------------------------------------------------
// Name: dump_stack
// Desc:
//------------------------------------------------------------------------------
void DumpState::dump_stack(const State &state) {
	dump_lines(state.stack_pointer(), 4);
}

//------------------------------------------------------------------------------
// Name: dump_data
// Desc:
//------------------------------------------------------------------------------
void DumpState::dump_data(edb::address_t address) {
	dump_lines(address, 2);
}

//------------------------------------------------------------------------------
// Name: show_menu
// Desc:
//------------------------------------------------------------------------------
void DumpState::show_menu() {

	if(IProcess *process = edb::v1::debugger_core->process()) {
		if(IThread::pointer thread = process->current_thread()) {
			State state;
			thread->get_state(&state);
		
			std::cout << "------------------------------------------------------------------------------\n";
			dump_registers(state);
			std::cout << "[" << hex_string(state["ss"]) << ":" << hex_string(state.stack_pointer()) << "]---------------------------------------------------------[stack]\n";
			dump_stack(state);
		
			const edb::address_t data_address = edb::v1::current_data_view_address();
			std::cout << "[" << hex_string(state["ds"]) << ":" << hex_string(data_address) << "]---------------------------------------------------------[ data]\n";
			dump_data(data_address);
			std::cout << "[" << hex_string(state["cs"]) << ":" << hex_string(state.instruction_pointer()) << "]---------------------------------------------------------[ code]\n";
			dump_code(state);
			std::cout << "------------------------------------------------------------------------------\n";
		}
	}
}

//------------------------------------------------------------------------------
// Name: options_page
// Desc:
//------------------------------------------------------------------------------
QWidget *DumpState::options_page() {
	return new OptionsPage;
}

#if QT_VERSION < 0x050000
Q_EXPORT_PLUGIN2(DumpState, DumpState)
#endif

}
