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
#include "IProcess.h"
#include "IThread.h"
#include "Instruction.h"
#include "OptionsPage.h"
#include "Register.h"
#include "State.h"
#include "Util.h"
#include "edb.h"

#include <QMenu>
#include <QSettings>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace DumpStatePlugin {
namespace {

/**
 * @brief hex_string
 * @param value
 * @return
 */
template <class T>
std::string hex_string(const T &value) {
	return value.toHexString().toStdString();
}

constexpr const char Reset[]  = "\x1B[0m";
constexpr const char Red[]    = "\x1B[91m";
constexpr const char Yellow[] = "\x1B[93m";
constexpr const char Cyan[]   = "\x1B[96m";
constexpr const char Blue[]   = "\x1B[94m";
constexpr const char Green[]  = "\x1B[92m";
constexpr const char Purple[] = "\x1B[95m";

/**
 * @brief format_register
 * @param value
 * @return
 */
template <class T>
std::string format_register(const T &value) {

	QSettings settings;
	const int colorize = settings.value("DumpState/colorize", true).toBool();

	if (colorize) {
		return Blue + hex_string(value) + Reset;
	} else {
		return hex_string(value);
	}
}

/**
 * @brief format_segment
 * @param value
 * @return
 */
template <class T>
std::string format_segment(const T &value) {

	QSettings settings;
	const int colorize = settings.value("DumpState/colorize", true).toBool();

	if (colorize) {
		return Green + hex_string(value) + Reset;
	} else {
		return hex_string(value);
	}
}

/**
 * @brief format_address
 * @param value
 * @return
 */
template <class T>
std::string format_address(const T &value) {

	QSettings settings;
	const int colorize = settings.value("DumpState/colorize", true).toBool();

	if (colorize) {
		return Purple + hex_string(value) + Reset;
	} else {
		return hex_string(value);
	}
}

}

/**
 * @brief DumpState::DumpState
 * @param parent
 */
DumpState::DumpState(QObject *parent)
	: QObject(parent) {
}

/**
 * @brief DumpState::menu
 * @param parent
 * @return
 */
QMenu *DumpState::menu(QWidget *parent) {

	Q_ASSERT(parent);

	if (!menu_) {
		menu_ = new QMenu(tr("DumpState"), parent);
		menu_->addAction(tr("&Dump Current State"), this, SLOT(showMenu()), QKeySequence(tr("Ctrl+D")));
	}

	return menu_;
}

/**
 * @brief DumpState::dumpCode
 * @param state
 */
void DumpState::dumpCode(const State &state) {

	QSettings settings;
	const int instructions_to_print = settings.value("DumpState/instructions_after_ip", 6).toInt();

	const edb::address_t ip = state.instructionPointer();
	edb::address_t address  = ip;

	for (int i = 0; i < instructions_to_print + 1; ++i) {
		uint8_t buf[edb::Instruction::MaxSize];
		if (const int size = edb::v1::get_instruction_bytes(address, buf)) {
			edb::Instruction inst(buf, buf + size, address);
			if (inst) {
				std::cout << ((address == ip) ? "> " : "  ") << hex_string(address) << ": " << edb::v1::formatter().toString(inst) << "\n";
			} else {
				break;
			}
			address += inst.byteSize();
		} else {
			break;
		}
	}
}

/**
 * @brief DumpState::dumpRegisters
 * @param state
 */
void DumpState::dumpRegisters(const State &state) {

	using std::cout;
	if (edb::v1::debuggeeIs32Bit()) { // TODO: check if state itself is 32 bit, not current debuggee. Generally it's not the same.
		cout << "     eax:" << format_register(state["eax"]);
		cout << " ecx:" << format_register(state["ecx"]);
		cout << "  edx:" << format_register(state["edx"]);
		cout << "  ebx:" << format_register(state["ebx"]);
		cout << "     eflags:" << format_register(state["eflags"]);
		cout << "\n";
		cout << "     esp:" << format_register(state["esp"]);
		cout << " ebp:" << format_register(state["ebp"]);
		cout << "  esi:" << format_register(state["esi"]);
		cout << "  edi:" << format_register(state["edi"]);
		cout << "     eip:" << format_register(state["eip"]);
		cout << "\n";
		cout << "     es:" << format_segment(state["es"]);
		cout << "  cs:" << format_segment(state["cs"]);
		cout << "  ss:" << format_segment(state["ss"]);
		cout << "  ds:" << format_segment(state["ds"]);
		cout << "  fs:" << format_segment(state["fs"]);
		cout << "  gs:" << format_segment(state["gs"]);
		cout << "    ";
		const Register eflagsR = state["eflags"];
		if (eflagsR) {
			const auto eflags = eflagsR.value<edb::value32>();
			cout << ((eflags & (1 << 11)) != 0 ? 'O' : 'o') << ' ';
			cout << ((eflags & (1 << 10)) != 0 ? 'D' : 'd') << ' ';
			cout << ((eflags & (1 << 9)) != 0 ? 'I' : 'i') << ' ';
			cout << ((eflags & (1 << 8)) != 0 ? 'T' : 't') << ' ';
			cout << ((eflags & (1 << 7)) != 0 ? 'S' : 's') << ' ';
			cout << ((eflags & (1 << 6)) != 0 ? 'Z' : 'z') << ' ';
			cout << ((eflags & (1 << 4)) != 0 ? 'A' : 'a') << ' ';
			cout << ((eflags & (1 << 2)) != 0 ? 'P' : 'p') << ' ';
			cout << ((eflags & (1 << 0)) != 0 ? 'C' : 'c');
		}
		cout << "\n";
	} else {
		cout << "     rax:" << format_register(state["rax"]);
		cout << " rcx:" << format_register(state["rcx"]);
		cout << "  rdx:" << format_register(state["rdx"]);
		cout << "  rbx:" << format_register(state["rbx"]);
		cout << "     rflags:" << format_register(state["rflags"]);
		cout << "\n";
		cout << "     rsp:" << format_register(state["rsp"]);
		cout << " rbp:" << format_register(state["rbp"]);
		cout << "  rsi:" << format_register(state["rsi"]);
		cout << "  rdi:" << format_register(state["rdi"]);
		cout << "        rip:" << format_register(state["rip"]);
		cout << "\n";
		cout << "      r8:" << format_register(state["r8"]);
		cout << "  r9:" << format_register(state["r9"]);
		cout << "  r10:" << format_register(state["r10"]);
		cout << "  r11:" << format_register(state["r11"]);
		cout << "           ";

		const Register rflagsR = state["rflags"];
		if (rflagsR) {
			const auto rflags = rflagsR.value<edb::value32>();
			cout << ((rflags & (1 << 11)) != 0 ? 'O' : 'o') << ' ';
			cout << ((rflags & (1 << 10)) != 0 ? 'D' : 'd') << ' ';
			cout << ((rflags & (1 << 9)) != 0 ? 'I' : 'i') << ' ';
			cout << ((rflags & (1 << 8)) != 0 ? 'T' : 't') << ' ';
			cout << ((rflags & (1 << 7)) != 0 ? 'S' : 's') << ' ';
			cout << ((rflags & (1 << 6)) != 0 ? 'Z' : 'z') << ' ';
			cout << ((rflags & (1 << 4)) != 0 ? 'A' : 'a') << ' ';
			cout << ((rflags & (1 << 2)) != 0 ? 'P' : 'p') << ' ';
			cout << ((rflags & (1 << 0)) != 0 ? 'C' : 'c');
		}

		cout << "\n";
		cout << "     r12:" << format_register(state["r12"]);
		cout << " r13:" << format_register(state["r13"]);
		cout << "  r14:" << format_register(state["r14"]);
		cout << "  r15:" << format_register(state["r15"]);
		cout << "\n";
		cout << "     es:" << format_segment(state["es"]);
		cout << "  cs:" << format_segment(state["cs"]);
		cout << "  ss:" << format_segment(state["ss"]);
		cout << "  ds:" << format_segment(state["ds"]);
		cout << "  fs:" << format_segment(state["fs"]);
		cout << "  gs:" << format_segment(state["gs"]);
		cout << "\n";
	}
}

/**
 * @brief DumpState::dumpLines
 * @param address
 * @param lines
 */
void DumpState::dumpLines(edb::address_t address, int lines) {

	if (IProcess *process = edb::v1::debugger_core->process()) {
		for (int i = 0; i < lines; ++i) {
			edb::value8 buf[16];
			if (process->readBytes(address, buf, sizeof(buf))) {

				std::cout << hex_string(address) << " : ";

				for (int j = 0x00; j < 0x04; ++j)
					std::cout << hex_string(buf[j]) << " ";
				std::cout << " ";

				for (int j = 0x04; j < 0x08; ++j)
					std::cout << hex_string(buf[j]) << " ";
				std::cout << "- ";
				for (int j = 0x08; j < 0x0c; ++j)
					std::cout << hex_string(buf[j]) << " ";
				std::cout << " ";
				for (int j = 0x0c; j < 0x10; ++j)
					std::cout << hex_string(buf[j]) << " ";

				for (int j = 0; j < 16; ++j) {
					// TODO(eteran): why won't this compile with MSVC?
					const uint8_t ch = buf[j].toUint();
					std::cout << ((std::isprint(ch) || (std::isspace(ch) && (ch != '\f' && ch != '\t' && ch != '\r' && ch != '\n' && ch != '\x0b') && ch < 0x80)) ? static_cast<char>(ch) : '.');
				}

				std::cout << "\n";
			} else {
				break;
			}
			address += 16;
		}
	}
}

/**
 * @brief DumpState::dumpStack
 * @param state
 */
void DumpState::dumpStack(const State &state) {
	dumpLines(state.stackPointer(), 4);
}

/**
 * @brief DumpState::dumpData
 * @param address
 */
void DumpState::dumpData(edb::address_t address) {
	dumpLines(address, 2);
}

/**
 * @brief DumpState::showMenu
 */
void DumpState::showMenu() {

	if (IProcess *process = edb::v1::debugger_core->process()) {
		if (std::shared_ptr<IThread> thread = process->currentThread()) {
			State state;
			thread->getState(&state);

			std::cout << "------------------------------------------------------------------------------\n";
			dumpRegisters(state);
			std::cout << "[" << format_segment(state["ss"]) << ":" << format_address(state.stackPointer()) << "]---------------------------------------------------------[stack]\n";
			dumpStack(state);

			const edb::address_t data_address = edb::v1::current_data_view_address();
			std::cout << "[" << format_segment(state["ds"]) << ":" << format_address(data_address) << "]---------------------------------------------------------[ data]\n";
			dumpData(data_address);
			std::cout << "[" << format_segment(state["cs"]) << ":" << format_address(state.instructionPointer()) << "]---------------------------------------------------------[ code]\n";
			dumpCode(state);
			std::cout << "------------------------------------------------------------------------------\n";
		}
	}
}

/**
 * @brief DumpState::optionsPage
 * @return
 */
QWidget *DumpState::optionsPage() {
	return new OptionsPage;
}

}
