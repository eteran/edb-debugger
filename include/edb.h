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

#ifndef DEBUGGER_20061101_H_
#define DEBUGGER_20061101_H_

#include "API.h"
#include "IBinary.h"
#include "IRegion.h"
#include "IBreakpoint.h"
#include "Types.h"
#include "Register.h"

#include <QMap>
#include <QList>
#include <QPointer>
#include <QStringList>
#include <QVector>

#include <memory>

class ArchProcessor;
class Configuration;
class IAnalyzer;
class IBinary;
class IDebugEventHandler;
class IDebugger;
class IPlugin;
class ISymbolManager;
class MemoryRegions;
class State;

class QAbstractScrollArea;
class QByteArray;
class QDialog;
class QFileInfo;
class QWidget;
class QString;

struct ExpressionError;

namespace edb {

struct Prototype;

namespace v1 {

// some useful objects
EDB_EXPORT extern IDebugger *debugger_core;
EDB_EXPORT extern QWidget       *debugger_ui;

// the symbol mananger
EDB_EXPORT ISymbolManager &symbol_manager();

// the memory region manager
EDB_EXPORT MemoryRegions &memory_regions();

// the current arch processor
EDB_EXPORT ArchProcessor &arch_processor();

// widgets
EDB_EXPORT QAbstractScrollArea *disassembly_widget();

// breakpoint managment
EDB_EXPORT IBreakpoint::pointer find_breakpoint(address_t address);
EDB_EXPORT QString get_breakpoint_condition(address_t address);
EDB_EXPORT address_t disable_breakpoint(address_t address);
EDB_EXPORT address_t enable_breakpoint(address_t address);
EDB_EXPORT IBreakpoint::pointer create_breakpoint(address_t address);
EDB_EXPORT void remove_breakpoint(address_t address);
EDB_EXPORT void set_breakpoint_condition(address_t address, const QString &condition);
EDB_EXPORT void toggle_breakpoint(address_t address);

EDB_EXPORT address_t current_data_view_address();

// change what the various views show
EDB_EXPORT bool dump_data_range(address_t address, address_t end_address, bool new_tab);
EDB_EXPORT bool dump_data_range(address_t address, address_t end_address);
EDB_EXPORT bool dump_data(address_t address, bool new_tab);
EDB_EXPORT bool dump_data(address_t address);
EDB_EXPORT bool dump_stack(address_t address, bool scroll_to);
EDB_EXPORT bool dump_stack(address_t address);
EDB_EXPORT bool jump_to_address(address_t address);

// ask the user for a value in an expression form
EDB_EXPORT bool get_expression_from_user(const QString &title, const QString prompt, address_t *value);
EDB_EXPORT bool eval_expression(const QString &expression, address_t *value);

// ask the user for a value suitable for a register via an input box
EDB_EXPORT bool get_value_from_user(Register &value, const QString &title);
EDB_EXPORT bool get_value_from_user(Register &value);

// ask the user for a binary string via an input box
EDB_EXPORT bool get_binary_string_from_user(QByteArray &value, const QString &title, int max_length);
EDB_EXPORT bool get_binary_string_from_user(QByteArray &value, const QString &title);

// determine if the given address is the starting point of an string, if so, s will contain it
// (formatted with C-style escape chars, so foundLength will have the original length of the string in chars).
EDB_EXPORT bool get_ascii_string_at_address(address_t address, QString &s, int min_length, int max_length, int &found_length);
EDB_EXPORT bool get_utf16_string_at_address(address_t address, QString &s, int min_length, int max_length, int &found_length);

EDB_EXPORT IRegion::pointer current_cpu_view_region();
EDB_EXPORT IRegion::pointer primary_code_region();
EDB_EXPORT IRegion::pointer primary_data_region();

// configuration
EDB_EXPORT QPointer<QDialog> dialog_options();
EDB_EXPORT Configuration &config();

// a numeric version of the current version suitable for integer comparison
EDB_EXPORT quint32 edb_version();
EDB_EXPORT quint32 int_version(const QString &s);

// symbol resolution
EDB_EXPORT QString find_function_symbol(address_t address);
EDB_EXPORT QString find_function_symbol(address_t address, const QString &default_value);
EDB_EXPORT QString find_function_symbol(address_t address, const QString &default_value, int *offset);

// ask the user for either a value or a variable (register name and such)
EDB_EXPORT address_t get_value(address_t address, bool *ok, ExpressionError *err);
EDB_EXPORT address_t get_variable(const QString &s, bool *ok, ExpressionError *err);

// hook the debug event system
EDB_EXPORT IDebugEventHandler *set_debug_event_handler(IDebugEventHandler *p);
EDB_EXPORT IDebugEventHandler *debug_event_handler();

EDB_EXPORT IAnalyzer *set_analyzer(IAnalyzer *p);
EDB_EXPORT IAnalyzer *analyzer();

// reads up to size bytes from address (stores how many it could read in size)
EDB_EXPORT bool get_instruction_bytes(address_t address, quint8 *buf, int *size);

template <int N>
EDB_EXPORT int get_instruction_bytes(address_t address, quint8 (&buffer)[N]) {
	int size = N;
	if(edb::v1::get_instruction_bytes(address, buffer, &size)) {
		return size;
	}

	return 0;
}

EDB_EXPORT QString disassemble_address(address_t address);

EDB_EXPORT std::unique_ptr<IBinary> get_binary_info(const IRegion::pointer &region);
EDB_EXPORT const Prototype *get_function_info(const QString &function);

EDB_EXPORT address_t locate_main_function();

EDB_EXPORT const QMap<QString, QObject *> &plugin_list();
EDB_EXPORT IPlugin *find_plugin_by_name(const QString &name);

EDB_EXPORT void reload_symbols();
EDB_EXPORT void repaint_cpu_view();
EDB_EXPORT void update_ui();

// these are here and not members of state because
// they may require using the debugger core plugin and
// we don't want to force a dependancy between the two
EDB_EXPORT void pop_value(State *state);
EDB_EXPORT void push_value(State *state, reg_t value);

EDB_EXPORT void register_binary_info(IBinary::create_func_ptr_t fptr);

EDB_EXPORT bool overwrite_check(address_t address, unsigned int size);
EDB_EXPORT bool modify_bytes(address_t address, unsigned int size, QByteArray &bytes, quint8 fill);

EDB_EXPORT QByteArray get_file_md5(const QString &s);
EDB_EXPORT QByteArray get_md5(const void *p, size_t n);
EDB_EXPORT QByteArray get_md5(const QVector<quint8> &bytes);

EDB_EXPORT QString symlink_target(const QString &s);
EDB_EXPORT QStringList parse_command_line(const QString &cmdline);
EDB_EXPORT address_t string_to_address(const QString &s, bool *ok);
EDB_EXPORT QString format_bytes(const QByteArray &x);
EDB_EXPORT QString format_bytes(quint8 byte);
EDB_EXPORT QString format_pointer(address_t p);

EDB_EXPORT address_t cpu_selected_address();
EDB_EXPORT void set_cpu_selected_address(address_t address);

EDB_EXPORT void set_status(const QString &message);

EDB_EXPORT int pointer_size();

EDB_EXPORT QVector<quint8> read_pages(address_t address, size_t page_count);

EDB_EXPORT CapstoneEDB::Formatter &formatter();

EDB_EXPORT bool debuggeeIs32Bit();
EDB_EXPORT bool debuggeeIs64Bit();

EDB_EXPORT address_t selected_stack_address();
EDB_EXPORT size_t    selected_stack_size();

EDB_EXPORT address_t selected_data_address();
EDB_EXPORT size_t    selected_data_size();
}
}

#endif
