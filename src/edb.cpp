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

#include "edb.h"
#include "ArchProcessor.h"
#include "BinaryString.h"
#include "Configuration.h"
#include "Debugger.h"
#include "DialogInputBinaryString.h"
#include "DialogInputValue.h"
#include "DialogOptions.h"
#include "Debugger.h"
#include "Expression.h"
#include "Prototype.h"
#include "IDebugger.h"
#include "IPlugin.h"
#include "MD5.h"
#include "MemoryRegions.h"
#include "QHexView"
#include "State.h"
#include "SymbolManager.h"
#include "version.h"

#include <QAction>
#include <QAtomicPointer>
#include <QByteArray>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QInputDialog>
#include <QMessageBox>
#include <QLabel>

#include <cctype>

IDebugger *edb::v1::debugger_core = 0;
QWidget   *edb::v1::debugger_ui   = 0;

namespace {

	typedef QList<IBinary::create_func_ptr_t> BinaryInfoList;

	QAtomicPointer<IDebugEventHandler> g_DebugEventHandler = 0;
	QAtomicPointer<IAnalyzer>          g_Analyzer          = 0;
	QMap<QString, QObject *>           g_GeneralPlugins;
	BinaryInfoList                     g_BinaryInfoList;
	CapstoneEDB::Formatter             g_Formatter;

	QHash<QString, edb::Prototype>     g_FunctionDB;

	Debugger *ui() {
		return qobject_cast<Debugger *>(edb::v1::debugger_ui);
	}

	bool function_symbol_base(edb::address_t address, QString *value, int *offset) {

		Q_ASSERT(value);
		Q_ASSERT(offset);

		bool ret = false;
		*offset = 0;
		const Symbol::pointer s = edb::v1::symbol_manager().find_near_symbol(address);
		if(s) {
			*value = s->name;
			*offset = address - s->address;
			ret = true;
		}
		return ret;
	}
}

namespace edb {
namespace internal {

//------------------------------------------------------------------------------
// Name: register_plugin
// Desc:
//------------------------------------------------------------------------------
bool register_plugin(const QString &filename, QObject *plugin) {
	if(!g_GeneralPlugins.contains(filename)) {
		g_GeneralPlugins[filename] = plugin;
		return true;
	}

	return false;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
void load_function_db() {
	QFile file(":/debugger/xml/functions.xml");
	QDomDocument doc;

	if(file.open(QIODevice::ReadOnly)) {
		if(doc.setContent(&file)) {
			QDomElement root = doc.firstChildElement("functions");
			QDomElement function = root.firstChildElement("function");
			for (; !function.isNull(); function = function.nextSiblingElement("function")) {


				Prototype func;
				func.name = function.attribute("name");
				func.type = function.attribute("type");

				QDomElement argument = function.firstChildElement("argument");
				for (; !argument.isNull(); argument = argument.nextSiblingElement("argument")) {

					Argument arg;
					arg.name = argument.attribute("name");
					arg.type = argument.attribute("type");
					func.arguments.push_back(arg);
				}

				g_FunctionDB[func.name] = func;
			}
		}
	}
}

}

QString address_t::toPointerString(bool createdFromNativePointer) const {
	if(v1::debuggeeIs32Bit()) {
		return "0x"+toHexString();
	} else {
		if(!createdFromNativePointer) // then we don't know value of upper dword
			return "0x????????"+value32(value_[0]).toHexString();
		else
			return "0x"+toHexString();
	}
}
QString address_t::toHexString() const {
	if(v1::debuggeeIs32Bit()) {
		if(value_[0]>0xffffffffull) {
			// Make erroneous bits visible
			QString string=value64::toHexString();
			string.insert(8,"]");
			return "["+string;
		}
		return value32(value_[0]).toHexString();
	}
	else return value64::toHexString();
}
void address_t::normalize() {
	if(v1::debuggeeIs32Bit())
		value_[0]&=0xffffffffull;
}

namespace v1 {

bool debuggeeIs32Bit() { return pointer_size()==sizeof(std::uint32_t); }
bool debuggeeIs64Bit() { return pointer_size()==sizeof(std::uint64_t); }

//------------------------------------------------------------------------------
// Name: set_cpu_selected_address
// Desc:
//------------------------------------------------------------------------------
void set_cpu_selected_address(address_t address) {
	ui()->ui.cpuView->setSelectedAddress(address);
	ui()->ui.cpuView->update();
}

//------------------------------------------------------------------------------
// Name: cpu_selected_address
// Desc:
//------------------------------------------------------------------------------
address_t cpu_selected_address() {
	return ui()->ui.cpuView->selectedAddress();
}

//------------------------------------------------------------------------------
// Name: current_cpu_view_region
// Desc:
//------------------------------------------------------------------------------
IRegion::pointer current_cpu_view_region() {
	return ui()->ui.cpuView->region();
}

//------------------------------------------------------------------------------
// Name: repaint_cpu_view
// Desc:
//------------------------------------------------------------------------------
void repaint_cpu_view() {
	Debugger *const gui = ui();
	Q_ASSERT(gui);
	gui->ui.cpuView->update();
}

//------------------------------------------------------------------------------
// Name: symbol_manager
// Desc:
//------------------------------------------------------------------------------
ISymbolManager &symbol_manager() {
	static SymbolManager g_SymbolManager;
	return g_SymbolManager;
}

//------------------------------------------------------------------------------
// Name: memory_regions
// Desc:
//------------------------------------------------------------------------------
MemoryRegions &memory_regions() {
	static MemoryRegions g_MemoryRegions;
	return g_MemoryRegions;
}

//------------------------------------------------------------------------------
// Name: arch_processor
// Desc:
//------------------------------------------------------------------------------
ArchProcessor &arch_processor() {
	static ArchProcessor g_ArchProcessor;
	return g_ArchProcessor;
}

//------------------------------------------------------------------------------
// Name: set_analyzer
// Desc:
//------------------------------------------------------------------------------
IAnalyzer *set_analyzer(IAnalyzer *p) {
	Q_ASSERT(p);
	return g_Analyzer.fetchAndStoreAcquire(p);
}

//------------------------------------------------------------------------------
// Name: analyzer
// Desc:
//------------------------------------------------------------------------------
IAnalyzer *analyzer() {
#if QT_VERSION >= 0x050000
	return g_Analyzer.load();
#else
	return g_Analyzer;
#endif
}

//------------------------------------------------------------------------------
// Name: set_debug_event_handler
// Desc:
//------------------------------------------------------------------------------
IDebugEventHandler *set_debug_event_handler(IDebugEventHandler *p) {
	Q_ASSERT(p);
	return g_DebugEventHandler.fetchAndStoreAcquire(p);
}

//------------------------------------------------------------------------------
// Name: debug_event_handler
// Desc:
//------------------------------------------------------------------------------
IDebugEventHandler *debug_event_handler() {
#if QT_VERSION >= 0x050000
	return g_DebugEventHandler.load();
#else
	return g_DebugEventHandler;
#endif
}

//------------------------------------------------------------------------------
// Name: jump_to_address
// Desc: sets the disassembly display to a given address, returning success
//       status
//------------------------------------------------------------------------------
bool jump_to_address(address_t address) {
	Debugger *const gui = ui();
	Q_ASSERT(gui);
	return gui->jump_to_address(address);
}

//------------------------------------------------------------------------------
// Name: dump_data_range
// Desc: shows a given address through a given end address in the data view,
//       optionally in a new tab
//------------------------------------------------------------------------------
bool dump_data_range(address_t address, address_t end_address, bool new_tab) {
	Debugger *const gui = ui();
	Q_ASSERT(gui);
	return gui->dump_data_range(address, end_address, new_tab);
}

//------------------------------------------------------------------------------
// Name: dump_data_range
// Desc: shows a given address through a given end address in the data view
//------------------------------------------------------------------------------
bool dump_data_range(address_t address, address_t end_address) {
	return dump_data_range(address, end_address, false);
}

//------------------------------------------------------------------------------
// Name: dump_stack
// Desc:
//------------------------------------------------------------------------------
bool dump_stack(address_t address) {
	return dump_stack(address, true);
}

//------------------------------------------------------------------------------
// Name: dump_stack
// Desc: shows a given address in the stack view
//------------------------------------------------------------------------------
bool dump_stack(address_t address, bool scroll_to) {
	Debugger *const gui = ui();
	Q_ASSERT(gui);
	return gui->dump_stack(address, scroll_to);
}

//------------------------------------------------------------------------------
// Name: dump_data
// Desc: shows a given address in the data view, optionally in a new tab
//------------------------------------------------------------------------------
bool dump_data(address_t address, bool new_tab) {
	Debugger *const gui = ui();
	Q_ASSERT(gui);
	return gui->dump_data(address, new_tab);
}

//------------------------------------------------------------------------------
// Name: dump_data
// Desc: shows a given address in the data view
//------------------------------------------------------------------------------
bool dump_data(address_t address) {
	return dump_data(address, false);
}

//------------------------------------------------------------------------------
// Name: set_breakpoint_condition
// Desc:
//------------------------------------------------------------------------------
void set_breakpoint_condition(address_t address, const QString &condition) {
	IBreakpoint::pointer bp = find_breakpoint(address);
	if(bp) {
		bp->condition = condition;
	}
}

//------------------------------------------------------------------------------
// Name: get_breakpoint_condition
// Desc:
//------------------------------------------------------------------------------
QString get_breakpoint_condition(address_t address) {
	QString ret;
	IBreakpoint::pointer bp = find_breakpoint(address);
	if(bp) {
		ret = bp->condition;
	}

	return ret;
}


//------------------------------------------------------------------------------
// Name: create_breakpoint
// Desc: adds a breakpoint at a given address
//------------------------------------------------------------------------------
IBreakpoint::pointer create_breakpoint(address_t address) {

	IBreakpoint::pointer bp;

	memory_regions().sync();
	if(IRegion::pointer region = memory_regions().find_region(address)) {
		int ret = QMessageBox::Yes;

		if(!region->executable() && config().warn_on_no_exec_bp) {
			ret = QMessageBox::question(
				0,
				QT_TRANSLATE_NOOP("edb", "Suspicious breakpoint"),
				QT_TRANSLATE_NOOP("edb",
					"You want to place a breakpoint in a non-executable region.\n"
					"An INT3 breakpoint set on data will not execute and may cause incorrect results or crashes.\n"
					"Do you really want to set a breakpoint here?"),
				QMessageBox::Yes, QMessageBox::No);
		} else {
			quint8 buffer[Instruction::MAX_SIZE + 1];
			if(const int size = get_instruction_bytes(address, buffer)) {
				Instruction inst(buffer, buffer + size, address);
				if(!inst) {
					ret = QMessageBox::question(
						0,
						QT_TRANSLATE_NOOP("edb", "Suspicious breakpoint"),
						QT_TRANSLATE_NOOP("edb",
							"It looks like you may be setting an INT3 breakpoint on data.\n"
							"An INT3 breakpoint set on data will not execute and may cause incorrect results or crashes.\n"
							"Do you really want to set a breakpoint here?"),
						QMessageBox::Yes, QMessageBox::No);
				}
			}
		}


		if(ret == QMessageBox::Yes) {
			bp = debugger_core->add_breakpoint(address);
			repaint_cpu_view();
		}


	} else {
		QMessageBox::information(
			0,
			QT_TRANSLATE_NOOP("edb", "Error Setting Breakpoint"),
			QT_TRANSLATE_NOOP("edb", "Sorry, but setting a breakpoint which is not in a valid region is not allowed."));
	}
	
	return bp;
}

//------------------------------------------------------------------------------
// Name: enable_breakpoint
// Desc:
//------------------------------------------------------------------------------
address_t enable_breakpoint(address_t address) {
	if(address != 0) {
		IBreakpoint::pointer bp = find_breakpoint(address);
		if(bp && bp->enable()) {
			return address;
		}
	}
	return 0;
}

//------------------------------------------------------------------------------
// Name: disable_breakpoint
// Desc:
//------------------------------------------------------------------------------
address_t disable_breakpoint(address_t address) {
	if(address != 0) {
		IBreakpoint::pointer bp = find_breakpoint(address);
		if(bp && bp->disable()) {
			return address;
		}
	}
	return 0;
}

//------------------------------------------------------------------------------
// Name: toggle_breakpoint
// Desc: toggles the existence of a breakpoint at a given address
//------------------------------------------------------------------------------
void toggle_breakpoint(address_t address) {
	if(find_breakpoint(address)) {
		remove_breakpoint(address);
	} else {
		create_breakpoint(address);
	}
}

//------------------------------------------------------------------------------
// Name: remove_breakpoint
// Desc: removes a breakpoint
//------------------------------------------------------------------------------
void remove_breakpoint(address_t address) {
	debugger_core->remove_breakpoint(address);
	repaint_cpu_view();
}

//------------------------------------------------------------------------------
// Name: eval_expression
// Desc:
//------------------------------------------------------------------------------
bool eval_expression(const QString &expression, address_t *value) {

	Q_ASSERT(value);

	Expression<address_t> expr(expression, get_variable, get_value);
	ExpressionError err;

	bool ok;
	const address_t address = expr.evaluate_expression(&ok, &err);
	if(ok) {
		*value = address;
		return true;
	} else {
		QMessageBox::information(debugger_ui, QT_TRANSLATE_NOOP("edb", "Error In Expression!"), err.what());
		return false;
	}
}

//------------------------------------------------------------------------------
// Name: get_expression_from_user
// Desc:
//------------------------------------------------------------------------------
bool get_expression_from_user(const QString &title, const QString prompt, address_t *value) {
	bool ok;
    const QString text = QInputDialog::getText(debugger_ui, title, prompt, QLineEdit::Normal, QString(), &ok);

	if(ok && !text.isEmpty()) {
		return eval_expression(text, value);
	}
	return false;
}

//------------------------------------------------------------------------------
// Name: get_value_from_user
// Desc:
//------------------------------------------------------------------------------
bool get_value_from_user(Register &value) {
	return get_value_from_user(value, QT_TRANSLATE_NOOP("edb", "Input Value"));
}

//------------------------------------------------------------------------------
// Name: get_value_from_user
// Desc:
//------------------------------------------------------------------------------
bool get_value_from_user(Register &value, const QString &title) {
	static auto dlg = new DialogInputValue(debugger_ui);
	bool ret = false;

	dlg->setWindowTitle(title);
	dlg->set_value(value);
	if(dlg->exec() == QDialog::Accepted) {
		value.setScalarValue(dlg->value());
		ret = true;
	}

	return ret;
}

//------------------------------------------------------------------------------
// Name: get_binary_string_from_user
// Desc:
//------------------------------------------------------------------------------
bool get_binary_string_from_user(QByteArray &value, const QString &title) {
	return get_binary_string_from_user(value, title, 10);
}

//------------------------------------------------------------------------------
// Name: get_binary_string_from_user
// Desc:
//------------------------------------------------------------------------------
bool get_binary_string_from_user(QByteArray &value, const QString &title, int max_length) {
	static auto dlg = new DialogInputBinaryString(debugger_ui);

	bool ret = false;

	dlg->setWindowTitle(title);

	BinaryString *const bs = dlg->binary_string();

	// set the max length BEFORE the value! (or else we truncate incorrectly)
	if(value.length() <= max_length) {
		bs->setMaxLength(max_length);
	}

	bs->setValue(value);

	if(dlg->exec() == QDialog::Accepted) {
		value = bs->value();
		ret = true;
	}

	return ret;
}

//------------------------------------------------------------------------------
// Name: dialog_options
// Desc: returns a pointer to the options dialog
//------------------------------------------------------------------------------
QPointer<QDialog> dialog_options() {
	static QPointer<QDialog> dialog = new DialogOptions(debugger_ui);
	return dialog;
}

//------------------------------------------------------------------------------
// Name: config
// Desc:
//------------------------------------------------------------------------------
Configuration &config() {
	static Configuration g_Configuration;
	return g_Configuration;
}

//------------------------------------------------------------------------------
// Name: get_ascii_string_at_address
// Desc: attempts to get a string at a given address whose length is >= min_length
//       and < max_length
// Note: strings are comprised of printable characters and whitespace.
// Note: found_length is needed because we replace characters which need an
//       escape char with the escape sequence (thus the resultant string may be
//       longer than the original). found_length is the original length.
//------------------------------------------------------------------------------
bool get_ascii_string_at_address(address_t address, QString &s, int min_length, int max_length, int &found_length) {

	bool is_string = false;

	if(debugger_core) {
		if(IProcess *process = debugger_core->process()) {
			s.clear();

			if(min_length <= max_length) {
				while(max_length--) {
					char ch;
					if(!process->read_bytes(address++, &ch, sizeof(ch))) {
						break;
					}

					const int ascii_char = static_cast<unsigned char>(ch);
					if(ascii_char < 0x80 && (std::isprint(ascii_char) || std::isspace(ascii_char))) {
						s += ch;
					} else {
						break;
					}
				}
			}

			is_string = s.length() >= min_length;

			if(is_string) {
				found_length = s.length();
				s.replace("\r", "\\r");
				s.replace("\n", "\\n");
				s.replace("\t", "\\t");
				s.replace("\v", "\\v");
				s.replace("\"", "\\\"");
			}
		}
	}

	return is_string;
}

//------------------------------------------------------------------------------
// Name: get_utf16_string_at_address
// Desc: attempts to get a string at a given address whose length os >= min_length
//       and < max_length
// Note: strings are comprised of printable characters and whitespace.
// Note: found_length is needed because we replace characters which need an
//       escape char with the escape sequence (thus the resultant string may be
//       longer than the original). found_length is the original length.
//------------------------------------------------------------------------------
bool get_utf16_string_at_address(address_t address, QString &s, int min_length, int max_length, int &found_length) {
	bool is_string = false;
	if(debugger_core) {
		if(IProcess *process = debugger_core->process()) {
			s.clear();

			if(min_length <= max_length) {
				while(max_length--) {

					quint16 val;
					if(!process->read_bytes(address, &val, sizeof(val))) {
						break;
					}

					address += sizeof(val);

					QChar ch(val);

					// for now, we only acknowledge ASCII chars encoded as unicode
					const int ascii_char = ch.toLatin1();
					if(ascii_char >= 0x20 && ascii_char < 0x80) {
						s += ch;
					} else {
						break;
					}
				}
			}

			is_string = s.length() >= min_length;

			if(is_string) {
				found_length = s.length();
				s.replace("\r", "\\r");
				s.replace("\n", "\\n");
				s.replace("\t", "\\t");
				s.replace("\v", "\\v");
				s.replace("\"", "\\\"");
			}
		}
	}
	return is_string;
}

//------------------------------------------------------------------------------
// Name: find_function_symbol
// Desc:
//------------------------------------------------------------------------------
QString find_function_symbol(address_t address, const QString &default_value, int *offset) {

	QString symname(default_value);
	int off;

	if(function_symbol_base(address, &symname, &off)) {
		symname = QString("%1+%2").arg(symname).arg(off, 0, 16);
		if(offset) {
			*offset = off;
		}
	}

	return symname;
}

//------------------------------------------------------------------------------
// Name: find_function_symbol
// Desc:
//------------------------------------------------------------------------------
QString find_function_symbol(address_t address, const QString &default_value) {
	return find_function_symbol(address, default_value, 0);
}

//------------------------------------------------------------------------------
// Name: find_function_symbol
// Desc:
//------------------------------------------------------------------------------
QString find_function_symbol(address_t address) {
	return find_function_symbol(address, QString(), 0);
}

//------------------------------------------------------------------------------
// Name: get_variable
// Desc:
//------------------------------------------------------------------------------
address_t get_variable(const QString &s, bool *ok, ExpressionError *err) {

	Q_ASSERT(debugger_core);
	Q_ASSERT(ok);
	Q_ASSERT(err);

	State state;
	debugger_core->get_state(&state);
	const Register reg = state.value(s);
	*ok = reg;
	if(!*ok) {
		*err = ExpressionError(ExpressionError::UNKNOWN_VARIABLE);
	}

	// FIXME: should this really return segment base, not selector?
	// FIXME: if it's really meant to return base, then need to check whether
	//        State::operator[]() returned valid Register
	if(reg.name() == "fs") {
		return state["fs_base"].valueAsAddress();
	} else if(reg.name() == "gs") {
		return state["gs_base"].valueAsAddress();
	}

	return reg.valueAsAddress();
}

//------------------------------------------------------------------------------
// Name: get_value
// Desc:
//------------------------------------------------------------------------------
address_t get_value(address_t address, bool *ok, ExpressionError *err) {

	Q_ASSERT(debugger_core);
	Q_ASSERT(ok);
	Q_ASSERT(err);

	address_t ret = 0;
	*ok = false;

	if(IProcess *process = edb::v1::debugger_core->process()) {
		*ok = process->read_bytes(address, &ret, edb::v1::pointer_size());

		if(!*ok) {
			*err = ExpressionError(ExpressionError::CANNOT_READ_MEMORY);
		}
	}

	return ret;
}

//------------------------------------------------------------------------------
// Name: get_instruction_bytes
// Desc: attempts to read at most size bytes, but will retry using smaller sizes as needed
//------------------------------------------------------------------------------
bool get_instruction_bytes(address_t address, quint8 *buf, int *size) {

	Q_ASSERT(debugger_core);
	Q_ASSERT(size);
	Q_ASSERT(*size >= 0);

	if(IProcess *process = edb::v1::debugger_core->process()) {
		bool ok = process->read_bytes(address, buf, *size);

		while(!ok && *size) {
			ok = process->read_bytes(address, buf, --(*size));
		}

		return ok;
	}

	return false;
}

//------------------------------------------------------------------------------
// Name: get_binary_info
// Desc: gets an object which knows how to analyze the binary file provided
//       or NULL if none-found.
// Note: the caller is responsible for deleting the object!
//------------------------------------------------------------------------------
std::unique_ptr<IBinary> get_binary_info(const IRegion::pointer &region) {
	for(IBinary::create_func_ptr_t f: g_BinaryInfoList) {
		std::unique_ptr<IBinary> p((*f)(region));

		if(p->validate_header()) {
			return p;
		}
	}

	return nullptr;
}

//------------------------------------------------------------------------------
// Name: locate_main_function
// Desc:
// Note: this currently only works for glibc linked elf files
//------------------------------------------------------------------------------
address_t locate_main_function() {

	if(debugger_core) {
		if(IProcess *process = debugger_core->process()) {
			const address_t address = process->code_address();
			memory_regions().sync();
			if(IRegion::pointer region = memory_regions().find_region(address)) {
				if(auto binfo = get_binary_info(region)) {
					const address_t main_func = binfo->calculate_main();
					if(main_func != 0) {
						return main_func;
					} else {
						return binfo->entry_point();
					}
				}
			}
		}
	}

	return 0;
}

//------------------------------------------------------------------------------
// Name: plugin_list
// Desc:
//------------------------------------------------------------------------------
const QMap<QString, QObject *> &plugin_list() {
	return g_GeneralPlugins;
}

//------------------------------------------------------------------------------
// Name: find_plugin_by_name
// Desc: gets a pointer to a plugin based on it's classname
//------------------------------------------------------------------------------
IPlugin *find_plugin_by_name(const QString &name) {
	for(QObject *p: g_GeneralPlugins) {
		if(name == p->metaObject()->className()) {
			return qobject_cast<IPlugin *>(p);
		}
	}
	return 0;
}

//------------------------------------------------------------------------------
// Name: reload_symbols
// Desc:
//------------------------------------------------------------------------------
void reload_symbols() {
	symbol_manager().clear();
}

//------------------------------------------------------------------------------
// Name: get_function_info
// Desc:
//------------------------------------------------------------------------------
const Prototype *get_function_info(const QString &function) {

	auto it = g_FunctionDB.find(function);
	if(it != g_FunctionDB.end()) {
		return &(it.value());
	}

	return 0;
}

//------------------------------------------------------------------------------
// Name: primary_data_region
// Desc: returns the main .data section of the main executable module
// Note: make sure that memory regions has been sync'd first or you will likely
//       get a null-region result
//------------------------------------------------------------------------------
IRegion::pointer primary_data_region() {

	if(debugger_core) {
		if(IProcess *process = debugger_core->process()) {
			const address_t address = process->data_address();
			memory_regions().sync();
			if(IRegion::pointer region = memory_regions().find_region(address)) {
				return region;
			}
		}
	}

	return IRegion::pointer();
}

//------------------------------------------------------------------------------
// Name: primary_code_region
// Desc: returns the main .text section of the main executable module
// Note: make sure that memory regions has been sync'd first or you will likely
//       get a null-region result
//------------------------------------------------------------------------------
IRegion::pointer primary_code_region() {

#if defined(Q_OS_LINUX)
	if(debugger_core) {
		if(IProcess *process = debugger_core->process()) {
			const address_t address = process->code_address();
			memory_regions().sync();
			if(IRegion::pointer region = memory_regions().find_region(address)) {
				return region;
			}
		}
	}

	return IRegion::pointer();
#else
	const QString process_executable = debugger_core->process_exe(debugger_core->pid());

	memory_regions().sync();
	const QList<IRegion::pointer> r = memory_regions().regions();
	for(const IRegion::pointer &region: r) {
		if(region->executable() && region->name() == process_executable) {
			return region;
		}
	}
	return IRegion::pointer();
#endif
}

//------------------------------------------------------------------------------
// Name: pop_value
// Desc:
//------------------------------------------------------------------------------
void pop_value(State *state) {
	Q_ASSERT(state);
	state->adjust_stack(sizeof(reg_t));
}

//------------------------------------------------------------------------------
// Name: push_value
// Desc:
//------------------------------------------------------------------------------
void push_value(State *state, reg_t value) {
	Q_ASSERT(state);

	if(IProcess *process = edb::v1::debugger_core->process()) {
		state->adjust_stack(- static_cast<int>(pointer_size()));
		process->write_bytes(state->stack_pointer(), &value, pointer_size());
	}
}

//------------------------------------------------------------------------------
// Name: register_binary_info
// Desc:
//------------------------------------------------------------------------------
void register_binary_info(IBinary::create_func_ptr_t fptr) {
	if(!g_BinaryInfoList.contains(fptr)) {
		g_BinaryInfoList.push_back(fptr);
	}
}

//------------------------------------------------------------------------------
// Name: edb_version
// Desc: returns an integer comparable version of our current version string
//------------------------------------------------------------------------------
quint32 edb_version() {
	return int_version(version);
}

//------------------------------------------------------------------------------
// Name: overwrite_check
// Desc:
//------------------------------------------------------------------------------
bool overwrite_check(address_t address, unsigned int size) {
	bool firstConflict = true;
	for(address_t addr = address; addr != (address + size); ++addr) {
		IBreakpoint::pointer bp = find_breakpoint(addr);

		if(bp && bp->enabled()) {
			if(firstConflict) {
				const int ret = QMessageBox::question(
						0,
						QT_TRANSLATE_NOOP("edb", "Overwritting breakpoint"),
						QT_TRANSLATE_NOOP("edb", "You are attempting to modify bytes which overlap with a software breakpoint. Doing this will implicitly remove any breakpoints which are a conflict. Are you sure you want to do this?"),
						QMessageBox::Yes,
						QMessageBox::No);

				if(ret == QMessageBox::No) {
					return false;
				}
				firstConflict = false;
			}

			remove_breakpoint(addr);
		}
	}
	return true;
}

//------------------------------------------------------------------------------
// Name: update_ui
// Desc:
//------------------------------------------------------------------------------
void update_ui() {
	// force a full update
	Debugger *const gui = ui();
	Q_ASSERT(gui);
	gui->update_gui();
}

//------------------------------------------------------------------------------
// Name: modify_bytes
// Desc:
//------------------------------------------------------------------------------
bool modify_bytes(address_t address, unsigned int size, QByteArray &bytes, quint8 fill) {

	if(!edb::v1::overwrite_check(address, size)) {
		return false;
	}

	if(IProcess *process = edb::v1::debugger_core->process()) {
		if(size != 0) {
			// fill bytes
			while(bytes.size() < static_cast<int>(size)) {
				bytes.push_back(fill);
			}

			process->write_bytes(address, bytes.data(), size);

			// do a refresh, not full update
			Debugger *const gui = ui();
			Q_ASSERT(gui);
			gui->refresh_gui();
		}
	}

	return true;
}

//------------------------------------------------------------------------------
// Name: get_md5
// Desc:
//------------------------------------------------------------------------------
QByteArray get_md5(const QVector<quint8> &bytes) {
	return get_md5(&bytes[0], bytes.size());
}

//------------------------------------------------------------------------------
// Name: get_md5
// Desc:
//------------------------------------------------------------------------------
QByteArray get_md5(const void *p, size_t n) {
	MD5 md5(p, n);

	// make a deep copy because MD5 is about to go out of scope
	return QByteArray(reinterpret_cast<const char *>(md5.digest()), 16);
}

//------------------------------------------------------------------------------
// Name: get_file_md5
// Desc: returns a byte array representing the MD5 of a file
//------------------------------------------------------------------------------
QByteArray get_file_md5(const QString &s) {

	QFile file(s);
	file.open(QIODevice::ReadOnly);
	if(file.isOpen()) {
		if(file.size() != 0) {
			const QByteArray file_bytes = file.readAll();
			return get_md5(file_bytes.data(), file_bytes.size());
		}
	}

	return QByteArray();
}


//------------------------------------------------------------------------------
// Name: symlink_target
// Desc:
//------------------------------------------------------------------------------
QString symlink_target(const QString &s) {
	return QFileInfo(s).symLinkTarget();
}

//------------------------------------------------------------------------------
// Name: int_version
// Desc: returns an integer comparable version of a version string in x.y.z
//       format, or 0 if error
//------------------------------------------------------------------------------
quint32 int_version(const QString &s) {

	ulong ret = 0;
	const QStringList list = s.split(".");
	if(list.size() == 3) {
		bool ok[3];
		const unsigned int maj = list[0].toUInt(&ok[0]);
		const unsigned int min = list[1].toUInt(&ok[1]);
		const unsigned int rev = list[2].toUInt(&ok[2]);
		if(ok[0] && ok[1] && ok[2]) {
			ret = (maj << 12) | (min << 8) | (rev);
		}
	}
	return ret;
}

//------------------------------------------------------------------------------
// Name: parse_command_line
// Desc:
//------------------------------------------------------------------------------
QStringList parse_command_line(const QString &cmdline) {

	QStringList args;
	QString arg;

	int bcount = 0;
	bool in_quotes = false;

	auto s = cmdline.begin();

	while(s != cmdline.end()) {
		if(!in_quotes && s->isSpace()) {

			// Close the argument and copy it
			args << arg;
			arg.clear();

			// skip the remaining spaces
			do {
				++s;
			} while(s->isSpace());

			// Start with a new argument
			bcount = 0;
		} else if(*s == '\\') {

			// '\\'
			arg += *s++;
			++bcount;

		} else if(*s == '"') {

			// '"'
			if((bcount & 1) == 0) {
				/* Preceded by an even number of '\', this is half that
				 * number of '\', plus a quote which we erase.
				 */

				arg.chop(bcount / 2);
				in_quotes = !in_quotes;
			} else {
				/* Preceded by an odd number of '\', this is half that
				 * number of '\' followed by a '"'
				 */

				arg.chop(bcount / 2 + 1);
				arg += '"';
			}

			++s;
			bcount = 0;
		} else {
			arg += *s++;
			bcount = 0;
		}
	}

	if(!arg.isEmpty()) {
		args << arg;
	}

	return args;
}

//------------------------------------------------------------------------------
// Name: string_to_address
// Desc:
//------------------------------------------------------------------------------
address_t string_to_address(const QString &s, bool *ok) {
	QString hex(s);
	hex.replace("0x","");
	return edb::address_t::fromHexString(hex.left(2*sizeof(edb::address_t)),ok);
}

//------------------------------------------------------------------------------
// Name: format_bytes
// Desc:
//------------------------------------------------------------------------------
QString format_bytes(const QByteArray &x) {
	QString bytes;

	if(!x.isEmpty()) {
		bytes.reserve(x.size() * 4);

		auto it = x.begin();

		char buf[4];
		qsnprintf(buf, sizeof(buf), "%02x", *it++ & 0xff);
		bytes += buf;

		while(it != x.end()) {
			qsnprintf(buf, sizeof(buf), " %02x", *it++ & 0xff);
			bytes += buf;
		}
	}

	return bytes;
}

//------------------------------------------------------------------------------
// Name: format_bytes
// Desc:
//------------------------------------------------------------------------------
QString format_bytes(quint8 byte) {
	QString bytes;

	bytes.reserve(4);

	char buf[4];
	qsnprintf(buf, sizeof(buf), "%02x", byte & 0xff);
	bytes += buf;

	return bytes;
}

//------------------------------------------------------------------------------
// Name: format_pointer
// Desc:
//------------------------------------------------------------------------------
QString format_pointer(address_t p) {
	if(debugger_core) {
		return debugger_core->format_pointer(p);
	}
	return QString();
}

//------------------------------------------------------------------------------
// Name: current_data_view_address
// Desc:
//------------------------------------------------------------------------------
address_t current_data_view_address() {
	return qobject_cast<QHexView *>(ui()->ui.tabWidget->currentWidget())->firstVisibleAddress();
}

//------------------------------------------------------------------------------
// Name: set_status
// Desc:
//------------------------------------------------------------------------------
void set_status(const QString &message) {
	ui()->ui.statusbar->showMessage(message, 2000);
}

//------------------------------------------------------------------------------
// Name: find_breakpoint
// Desc:
//------------------------------------------------------------------------------
IBreakpoint::pointer find_breakpoint(address_t address) {
	if(debugger_core) {
		return debugger_core->find_breakpoint(address);
	}
	return IBreakpoint::pointer();
}

//------------------------------------------------------------------------------
// Name: pointer_size
// Desc:
//------------------------------------------------------------------------------
int pointer_size() {

	if(debugger_core) {
		return debugger_core->pointer_size();
	}

	// default to sizeof the native pointer for sanity!
	return sizeof(void*);
}

//------------------------------------------------------------------------------
// Name: disassembly_widget
// Desc:
//------------------------------------------------------------------------------
QAbstractScrollArea *disassembly_widget() {
	return ui()->ui.cpuView;
}

//------------------------------------------------------------------------------
// Name: read_pages
// Desc:
//------------------------------------------------------------------------------
QVector<quint8> read_pages(address_t address, size_t page_count) {

	if(debugger_core) {
		if(IProcess *process = edb::v1::debugger_core->process()) {
			try {
				const address_t page_size = debugger_core->page_size();
				QVector<quint8> pages(page_count * page_size);

				if(process->read_pages(address, pages.data(), page_count)) {
					return pages;
				}


			} catch(const std::bad_alloc &) {
				QMessageBox::information(0,
					QT_TRANSLATE_NOOP("edb", "Memroy Allocation Error"),
					QT_TRANSLATE_NOOP("edb", "Unable to satisfy memory allocation request for requested region->"));
			}
		}
	}

	return QVector<quint8>();
}

//------------------------------------------------------------------------------
// Name: disassemble_address
// Desc: will return a QString where isNull is true on failure
//------------------------------------------------------------------------------
QString disassemble_address(address_t address) {
	quint8 buffer[edb::Instruction::MAX_SIZE];
	if(const int size = edb::v1::get_instruction_bytes(address, buffer)) {
		edb::Instruction inst(buffer, buffer + size, address);
		if(inst) {
			return QString::fromStdString(g_Formatter.to_string(inst));
		}
	}

	return QString();
}

//------------------------------------------------------------------------------
// Name: formatter
// Desc: returns a reference to the global instruction formatter
//------------------------------------------------------------------------------
CapstoneEDB::Formatter &formatter() {
	return g_Formatter;
}

//------------------------------------------------------------------------------
// Name: selected_stack_address
// Desc: returns the address of the selection or (address_t)-1
//------------------------------------------------------------------------------
address_t selected_stack_address() {

	if(auto hexview = qobject_cast<QHexView *>(ui()->ui.stackDock->widget())) {
		if(hexview->hasSelectedText()) {
			return hexview->selectedBytesAddress();
		}
	}
	
	return static_cast<address_t>(-1);
}

//------------------------------------------------------------------------------
// Name: selected_stack_size
// Desc: returns the size of the selection or 0
//------------------------------------------------------------------------------
size_t selected_stack_size() {
	if(auto hexview = qobject_cast<QHexView *>(ui()->ui.stackDock->widget())) {
		if(hexview->hasSelectedText()) {
			return hexview->selectedBytesSize();
		}
	}
	
	return 0;
}

//------------------------------------------------------------------------------
// Name: selected_stack_address
// Desc: returns the address of the selection or (address_t)-1
//------------------------------------------------------------------------------
address_t selected_data_address() {
	if(auto hexview = qobject_cast<QHexView *>(ui()->ui.tabWidget->currentWidget())) {
		if(hexview->hasSelectedText()) {
			return hexview->selectedBytesAddress();
		}
	}
	
	return static_cast<address_t>(-1);
}

//------------------------------------------------------------------------------
// Name: selected_data_size
// Desc: returns the size of the selection or 0
//------------------------------------------------------------------------------
size_t selected_data_size() {
	if(auto hexview = qobject_cast<QHexView *>(ui()->ui.tabWidget->currentWidget())) {
		if(hexview->hasSelectedText()) {
			return hexview->selectedBytesSize();
		}
	}
	
	return 0;
}

}
}
